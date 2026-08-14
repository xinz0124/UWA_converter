#include <cJSON.h>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;
using json_object = cJSON;

static size_t json_object_array_length(const json_object *a) {
    return a ? static_cast<size_t>(cJSON_GetArraySize(a)) : 0;
}
static json_object *json_object_array_get_idx(const json_object *a, size_t i) {
    return a ? cJSON_GetArrayItem(a, static_cast<int>(i)) : nullptr;
}
static int json_object_array_add(json_object *a, json_object *v) {
    return cJSON_AddItemToArray(a, v);
}
static void json_object_array_put_idx(json_object *a, size_t i, json_object *v) {
    cJSON_ReplaceItemInArray(a, static_cast<int>(i), v);
}
static void json_object_array_del_idx(json_object *a, size_t i, size_t count) {
    while (count--)
        cJSON_DeleteItemFromArray(a, static_cast<int>(i));
}
static json_object *json_object_new_array() { return cJSON_CreateArray(); }
static json_object *json_object_new_object() { return cJSON_CreateObject(); }
static json_object *json_object_new_string(const char *s) { return cJSON_CreateString(s); }
static json_object *json_object_new_double(double x) { return cJSON_CreateNumber(x); }
static json_object *json_object_new_uint64(uint64_t x) {
    return cJSON_CreateNumber(static_cast<double>(x));
}
static json_object *json_object_new_boolean(int value) { return cJSON_CreateBool(value); }
static double json_object_get_double(const json_object *o) { return o ? o->valuedouble : 0; }
static uint64_t json_object_get_uint64(const json_object *o) {
    return o ? static_cast<uint64_t>(o->valuedouble) : 0;
}
static const char *json_object_get_string(const json_object *o) {
    return cJSON_IsString(o) && o->valuestring ? o->valuestring : "";
}
static void json_object_object_add(json_object *o, const char *k, json_object *v) {
    if (cJSON_HasObjectItem(o, k))
        cJSON_ReplaceItemInObjectCaseSensitive(o, k, v);
    else
        cJSON_AddItemToObject(o, k, v);
}
static void json_object_object_del(json_object *o, const char *k) {
    if (o)
        cJSON_DeleteItemFromObjectCaseSensitive(o, k);
}
static void json_object_put(json_object *o) { cJSON_Delete(o); }
static uint32_t le32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
static uint32_t be32(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
static void putle(Bytes &b, uint32_t v) {
    for (int i = 0; i < 4; i++)
        b.push_back(v >> (8 * i));
}
static void putbe(Bytes &b, uint32_t v) {
    for (int i = 3; i >= 0; i--)
        b.push_back(v >> (8 * i));
}
static void setbe(Bytes &b, size_t p, uint32_t v) {
    for (int i = 0; i < 4; i++)
        b[p + i] = v >> (24 - 8 * i);
}
static Bytes readFile(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("无法读取: " + p);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}
static void writeFile(const std::string &p, const Bytes &b) {
    std::ofstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("无法写入: " + p);
    f.write((const char *)b.data(), b.size());
    if (!f)
        throw std::runtime_error("写入失败: " + p);
}
static json_object *get(json_object *o, const char *k) {
    return o ? cJSON_GetObjectItemCaseSensitive(o, k) : nullptr;
}
static void renameKey(json_object *o, const std::string &a, const std::string &b) {
    if (auto *v = cJSON_DetachItemFromObjectCaseSensitive(o, a.c_str())) {
        json_object_object_add(o, b.c_str(), v);
    }
}
static void renameArrayValues(json_object *a, const std::string &from, const std::string &to) {
    if (!a)
        return;
    for (size_t i = 0; i < json_object_array_length(a); i++) {
        auto *v = json_object_array_get_idx(a, i);
        if (cJSON_IsString(v) && from == json_object_get_string(v))
            json_object_array_put_idx(a, i, json_object_new_string(to.c_str()));
    }
}
static void appendArrayValueIfMissing(json_object *a, const std::string &value) {
    if (!a)
        return;
    for (size_t i = 0; i < json_object_array_length(a); i++) {
        auto *v = json_object_array_get_idx(a, i);
        if (cJSON_IsString(v) && value == json_object_get_string(v))
            return;
    }
    json_object_array_add(a, json_object_new_string(value.c_str()));
}
static json_object *num(double x) {
    return json_object_new_double(x);
}
static json_object *vec3(double a, double b, double c) {
    auto *x = json_object_new_array();
    json_object_array_add(x, num(a));
    json_object_array_add(x, num(b));
    json_object_array_add(x, num(c));
    return x;
}
static double at(json_object *a, int i) {
    return json_object_get_double(json_object_array_get_idx(a, i));
}

static bool numericArray(json_object *a, size_t length) {
    if (!cJSON_IsArray(a) || json_object_array_length(a) != length)
        return false;
    for (size_t i = 0; i < length; i++) {
        if (!cJSON_IsNumber(json_object_array_get_idx(a, i)))
            return false;
    }
    return true;
}

static json_object *matrixToQuaternion(json_object *matrix) {
    const double m00 = at(matrix, 0), m01 = at(matrix, 1), m02 = at(matrix, 2);
    const double m10 = at(matrix, 3), m11 = at(matrix, 4), m12 = at(matrix, 5);
    const double m20 = at(matrix, 6), m21 = at(matrix, 7), m22 = at(matrix, 8);
    double x, y, z, w;
    const double trace = m00 + m11 + m22;
    if (trace > 0) {
        const double s = 2 * std::sqrt(trace + 1);
        w = s / 4;
        x = (m21 - m12) / s;
        y = (m02 - m20) / s;
        z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const double s = 2 * std::sqrt(1 + m00 - m11 - m22);
        w = (m21 - m12) / s;
        x = s / 4;
        y = (m01 + m10) / s;
        z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const double s = 2 * std::sqrt(1 + m11 - m00 - m22);
        w = (m02 - m20) / s;
        x = (m01 + m10) / s;
        y = s / 4;
        z = (m12 + m21) / s;
    } else {
        const double s = 2 * std::sqrt(1 + m22 - m00 - m11);
        w = (m10 - m01) / s;
        x = (m02 + m20) / s;
        y = (m12 + m21) / s;
        z = s / 4;
    }
    const double length = std::sqrt(x * x + y * y + z * z + w * w);
    auto *result = json_object_new_array();
    for (double value : {x / length, y / length, z / length, w / length})
        json_object_array_add(result, num(value));
    return result;
}

static json_object *quaternionToMatrix(json_object *quaternion) {
    double x = at(quaternion, 0), y = at(quaternion, 1), z = at(quaternion, 2),
           w = at(quaternion, 3);
    const double length = std::sqrt(x * x + y * y + z * z + w * w);
    if (length > 0)
        x /= length, y /= length, z /= length, w /= length;
    else
        x = y = z = 0, w = 1;
    const double values[] = {
        1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
        2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
        2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
    auto *result = json_object_new_array();
    for (double value : values)
        json_object_array_add(result, num(std::abs(value) < 1e-15 ? 0 : value));
    return result;
}

static json_object *identityMatrix() {
    auto *result = json_object_new_array();
    for (int i = 0; i < 9; i++)
        json_object_array_add(result, num(i % 4 == 0));
    return result;
}

static bool isIdentityMatrix(json_object *matrix) {
    if (!numericArray(matrix, 9))
        return false;
    for (int i = 0; i < 9; i++)
        if (std::abs(at(matrix, i) - (i % 4 == 0 ? 1.0 : 0.0)) > 1e-12)
            return false;
    return true;
}

static void attributes(json_object *root, bool to10) {
    auto *meshes = get(root, "meshes");
    if (!meshes)
        return;
    for (size_t i = 0; i < json_object_array_length(meshes); i++) {
        auto *ps = get(json_object_array_get_idx(meshes, i), "primitives");
        if (!ps)
            continue;
        for (size_t j = 0; j < json_object_array_length(ps); j++) {
            auto *p = json_object_array_get_idx(ps, j);
            auto *a = get(p, "attributes");
            if (a) {
                std::vector<std::pair<std::string, std::string>> rs;
                cJSON *v = nullptr;
                cJSON_ArrayForEach(v, a) {
                    std::string s = v->string, t;
                    if (to10 && s == "_OPACITY")
                        t = "KHR_gaussian_splatting:OPACITY";
                    else if (to10 && s == "_SCALE")
                        t = "KHR_gaussian_splatting:SCALE";
                    else if (to10 && s == "_ROTATION")
                        t = "KHR_gaussian_splatting:ROTATION";
                    else if (to10 && s.rfind("_SPHERICAL_HARMONICS_DEGREE_", 0) == 0) {
                        t = s.substr(std::string("_SPHERICAL_HARMONICS_DEGREE_").size());
                        auto q = t.find("_COEFFICIENT_");
                        t = "KHR_gaussian_splatting:SH_DEGREE_" + t.substr(0, q) + "_COEF_" +
                            t.substr(q + 13);
                    } else if (!to10 && s == "KHR_gaussian_splatting:OPACITY")
                        t = "_OPACITY";
                    else if (!to10 && s == "KHR_gaussian_splatting:SCALE")
                        t = "_SCALE";
                    else if (!to10 && s == "KHR_gaussian_splatting:ROTATION")
                        t = "_ROTATION";
                    else if (!to10 && s.rfind("KHR_gaussian_splatting:SH_DEGREE_", 0) == 0) {
                        auto z = s.substr(std::string("KHR_gaussian_splatting:SH_DEGREE_").size());
                        auto q = z.find("_COEF_");
                        t = "_SPHERICAL_HARMONICS_DEGREE_" + z.substr(0, q) + "_COEFFICIENT_" +
                            z.substr(q + 6);
                    }
                    if (!t.empty())
                        rs.push_back({s, t});
                }
                for (auto &r : rs)
                    renameKey(a, r.first, r.second);
            }
            auto *ex = get(p, "extensions");
            if (!ex)
                ex = json_object_new_object(), json_object_object_add(p, "extensions", ex);
            if (to10) {
                auto *old = get(ex, "UWA_primitive_3DGS_compression");
                if (old) {
                    old = cJSON_DetachItemFromObjectCaseSensitive(
                        ex, "UWA_primitive_3DGS_compression");
                    auto *k = json_object_new_object();
                    json_object_object_add(k, "colorSpace",
                                           json_object_new_string("srgb_rec709_display"));
                    json_object_object_add(k, "kernel", json_object_new_string("ellipse"));
                    json_object_object_add(k, "sortingMethod",
                                           json_object_new_string("cameraDistance"));
                    json_object_object_add(k, "projection", json_object_new_string("perspective"));
                    auto *e = json_object_new_object();
                    json_object_object_add(e, "UWA_gaussian_splatting_compression_EGSC", old);
                    json_object_object_add(k, "extensions", e);
                    json_object_object_add(ex, "KHR_gaussian_splatting", k);
                }
            } else {
                auto *k = get(ex, "KHR_gaussian_splatting"), *e = get(k, "extensions"),
                     *old = get(e, "UWA_gaussian_splatting_compression_EGSC");
                if (old) {
                    old = cJSON_Duplicate(old, true);
                    json_object_object_del(ex, "KHR_gaussian_splatting");
                    json_object_object_add(ex, "UWA_primitive_3DGS_compression", old);
                }
            }
        }
    }
}
static void cameras(json_object *root, bool to10) {
    auto *nodes = get(root, "nodes");
    if (!nodes)
        return;
    auto *modelNode = json_object_array_length(nodes) ? json_object_array_get_idx(nodes, 0)
                                                       : nullptr;
    constexpr double pi = 3.14159265358979323846;
    for (size_t i = 0; i < json_object_array_length(nodes); i++) {
        auto *n = json_object_array_get_idx(nodes, i);
        if (!get(n, "camera"))
            continue;
        auto *e = get(n, "extensions");
        if (!e)
            e = json_object_new_object(), json_object_object_add(n, "extensions", e);
        if (to10) {
            auto *v = get(e, "UWA_viewing_parameters");
            // Keep the 0.4 viewing parameters in the 1.0 result for backward
            // compatibility. If constraints already exist, they are authoritative.
            if (v && !get(e, "UWA_viewing_constraints")) {
                auto *m = json_object_new_object(), *modes = json_object_new_array(),
                     *mode = json_object_new_object(), *six = json_object_new_object();
                auto *az = get(v, "longitudeRange"), *po = get(v, "latitudeRange");
                if (az) {
                    auto *x = json_object_new_array();
                    json_object_array_add(x, num(at(az, 0) * pi / 180));
                    json_object_array_add(x, num(at(az, 1) * pi / 180));
                    json_object_object_add(six, "azimuthRange", x);
                }
                if (po) {
                    auto *x = json_object_new_array();
                    json_object_array_add(x, num(at(po, 0) * pi / 180));
                    json_object_array_add(x, num(at(po, 1) * pi / 180));
                    json_object_object_add(six, "polarRange", x);
                }
                for (auto k : {"distanceRange", "target"})
                    if (auto *x = get(v, k)) {
                        json_object_object_add(six, k, cJSON_Duplicate(x, true));
                    }
                if (auto *b = get(v, "boundingBoxRange")) {
                    auto *x = get(b, "x"), *y = get(b, "y"), *z = get(b, "z");
                    if (x && y && z) {
                        auto *bb = json_object_new_object();
                        json_object_object_add(bb, "center",
                                               vec3((at(x, 0) + at(x, 1)) / 2,
                                                    (at(y, 0) + at(y, 1)) / 2,
                                                    (at(z, 0) + at(z, 1)) / 2));
                        json_object_object_add(
                            bb, "size",
                            vec3(at(x, 1) - at(x, 0), at(y, 1) - at(y, 0), at(z, 1) - at(z, 0)));
                        json_object_object_add(six, "targetBoundingBox", bb);
                    }
                }
                json_object_object_add(mode, "type", json_object_new_string("allocentricSixDof"));
                json_object_object_add(mode, "allocentricSixDof", six);
                json_object_array_add(modes, mode);
                json_object_object_add(m, "modes", modes);
                auto *gravity = get(v, "gravityCoordinateSystem");
                if (modelNode && numericArray(gravity, 9) && !isIdentityMatrix(gravity))
                    json_object_object_add(modelNode, "rotation", matrixToQuaternion(gravity));
                json_object_object_add(e, "UWA_viewing_constraints", m);
            }
            if (!get(e, "UWA_user_camera_label")) {
                auto *l = json_object_new_object(), *d = json_object_new_array();
                json_object_object_add(l, "default", json_object_new_boolean(1));
                json_object_array_add(d, json_object_new_string("phone"));
                json_object_array_add(d, json_object_new_string("pc"));
                json_object_object_add(l, "devices", d);
                json_object_object_add(e, "UWA_user_camera_label", l);
            }
        } else {
            auto *v = get(e, "UWA_viewing_constraints"), *modes = get(v, "modes"),
                 *mode = modes && json_object_array_length(modes)
                             ? json_object_array_get_idx(modes, 0)
                             : nullptr,
                 *type = get(mode, "type"),
                 *six = get(mode, "allocentricSixDof");
            // Only allocentricSixDof has a defined 0.4 representation.  Also
            // preserve an existing 0.4 extension instead of replacing it.
            if (!get(e, "UWA_viewing_parameters") && type &&
                cJSON_IsString(type) &&
                std::string(json_object_get_string(type)) == "allocentricSixDof" && six) {
                auto *o = json_object_new_object(), *az = get(six, "azimuthRange"),
                     *po = get(six, "polarRange");
                if (az) {
                    auto *x = json_object_new_array();
                    json_object_array_add(x, num(at(az, 0) * 180 / pi));
                    json_object_array_add(x, num(at(az, 1) * 180 / pi));
                    json_object_object_add(o, "longitudeRange", x);
                }
                if (po) {
                    auto *x = json_object_new_array();
                    json_object_array_add(x, num(at(po, 0) * 180 / pi));
                    json_object_array_add(x, num(at(po, 1) * 180 / pi));
                    json_object_object_add(o, "latitudeRange", x);
                }
                for (auto k : {"distanceRange", "target"})
                    if (auto *x = get(six, k)) {
                        json_object_object_add(o, k, cJSON_Duplicate(x, true));
                    }
                auto *rotation = modelNode ? get(modelNode, "rotation") : nullptr;
                auto *g = numericArray(rotation, 4) ? quaternionToMatrix(rotation)
                                                    : identityMatrix();
                json_object_object_add(o, "gravityCoordinateSystem", g);
                if (auto *b = get(six, "targetBoundingBox")) {
                    auto *c = get(b, "center"), *s = get(b, "size");
                    if (c && s) {
                        auto *r = json_object_new_object();
                        const char *ks[] = {"x", "y", "z"};
                        for (int q = 0; q < 3; q++) {
                            auto *a = json_object_new_array();
                            json_object_array_add(a, num(at(c, q) - at(s, q) / 2));
                            json_object_array_add(a, num(at(c, q) + at(s, q) / 2));
                            json_object_object_add(r, ks[q], a);
                        }
                        json_object_object_add(o, "boundingBoxRange", r);
                    }
                }
                json_object_object_del(e, "UWA_viewing_constraints");
                json_object_object_add(e, "UWA_viewing_parameters", o);
                if (modelNode && numericArray(rotation, 4))
                    json_object_object_del(modelNode, "rotation");
            }
            json_object_object_del(e, "UWA_user_camera_label");
        }
    }
}
static bool hasCameraExtension(json_object *root, const char *name) {
    auto *nodes = get(root, "nodes");
    if (!nodes)
        return false;
    for (size_t i = 0; i < json_object_array_length(nodes); i++) {
        auto *node = json_object_array_get_idx(nodes, i);
        if (get(node, "camera") && get(get(node, "extensions"), name))
            return true;
    }
    return false;
}
static void extLists(json_object *r, bool t) {
    auto *u = get(r, "extensionsUsed"), *q = get(r, "extensionsRequired");
    if (t) {
        for (auto *a : {u, q}) {
            renameArrayValues(a, "UWA_primitive_3DGS_compression",
                              "UWA_gaussian_splatting_compression_EGSC");
            if (hasCameraExtension(r, "UWA_viewing_parameters"))
                appendArrayValueIfMissing(a, "UWA_viewing_constraints");
            else
                renameArrayValues(a, "UWA_viewing_parameters", "UWA_viewing_constraints");
        }
        if (u) {
            json_object_array_add(u, json_object_new_string("KHR_gaussian_splatting"));
            json_object_array_add(u, json_object_new_string("UWA_user_camera_label"));
        }
    } else {
        for (auto *a : {u, q}) {
            renameArrayValues(a, "UWA_gaussian_splatting_compression_EGSC",
                              "UWA_primitive_3DGS_compression");
            // Unsupported modes (and a target extension that already existed)
            // leave the 1.0 extension intact, so its declaration must stay too.
            if (!hasCameraExtension(r, "UWA_viewing_constraints"))
                renameArrayValues(a, "UWA_viewing_constraints", "UWA_viewing_parameters");
        } /* harmless unknown 1.0 extension declarations are removed */
        for (auto *a : {u, q})
            if (a)
                for (int i = (int)json_object_array_length(a) - 1; i >= 0; i--) {
                    std::string s = json_object_get_string(json_object_array_get_idx(a, i));
                    if (s == "KHR_gaussian_splatting" || s == "UWA_user_camera_label" ||
                        s == "UWA_camera_trajectory_label" || s == "KHR_draco_mesh_compression")
                        json_object_array_del_idx(a, i, 1);
                }
    }
}
static Bytes replaceCompressedStream(json_object *root, const Bytes &oldBin,
                                     const Bytes &targetStream) {
    auto *views = get(root, "bufferViews");
    auto *buffers = get(root, "buffers");
    if (!views || json_object_array_length(views) == 0)
        throw std::runtime_error("GLB 缺少 bufferViews[0]");

    auto *streamView = json_object_array_get_idx(views, 0);
    auto *lengthObject = get(streamView, "byteLength");
    if (!lengthObject)
        throw std::runtime_error("bufferView 0 缺少 byteLength");

    const size_t oldOffset =
        get(streamView, "byteOffset") ? json_object_get_uint64(get(streamView, "byteOffset")) : 0;
    const size_t oldLength = json_object_get_uint64(lengthObject);
    if (oldOffset + oldLength > oldBin.size())
        throw std::runtime_error("bufferView 0 超出 GLB BIN chunk");

    Bytes paddedStream = targetStream;
    while (paddedStream.size() % 4)
        paddedStream.push_back(0);
    const int64_t delta = int64_t(paddedStream.size()) - int64_t(oldLength);

    Bytes newBin;
    newBin.reserve(size_t(int64_t(oldBin.size()) + delta));
    newBin.insert(newBin.end(), oldBin.begin(), oldBin.begin() + oldOffset);
    newBin.insert(newBin.end(), paddedStream.begin(), paddedStream.end());
    newBin.insert(newBin.end(), oldBin.begin() + oldOffset + oldLength, oldBin.end());

    json_object_object_add(streamView, "byteLength", json_object_new_uint64(paddedStream.size()));
    for (size_t i = 1; i < json_object_array_length(views); i++) {
        auto *view = json_object_array_get_idx(views, i);
        auto *offsetObject = get(view, "byteOffset");
        if (!offsetObject)
            continue;
        const uint64_t offset = json_object_get_uint64(offsetObject);
        if (offset >= oldOffset + oldLength) {
            const int64_t shifted = int64_t(offset) + delta;
            if (shifted < 0)
                throw std::runtime_error("bufferView byteOffset 计算结果无效");
            json_object_object_add(view, "byteOffset", json_object_new_uint64(shifted));
        }
    }

    if (!buffers || json_object_array_length(buffers) == 0)
        throw std::runtime_error("GLB 缺少 buffers[0]");
    auto *buffer = json_object_array_get_idx(buffers, 0);
    json_object_object_add(buffer, "byteLength", json_object_new_uint64(newBin.size()));
    return newBin;
}

static Bytes convertGlb(const Bytes &in, bool to10, const Bytes &targetStream) {
    if (in.size() < 20 || std::string((char *)in.data(), 4) != "glTF")
        throw std::runtime_error("不是有效 GLB");
    uint32_t jl = le32(&in[12]);
    if (20ull + jl > in.size() || le32(&in[16]) != 0x4E4F534A)
        throw std::runtime_error("GLB JSON chunk 无效");
    std::string s((char *)&in[20], jl);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
        s.pop_back();
    json_object *r = cJSON_ParseWithLength(s.data(), s.size());
    if (!r)
        throw std::runtime_error("JSON 解析失败");
    const size_t binChunkHeader = 20ull + jl;
    if (binChunkHeader + 8 > in.size() || le32(&in[binChunkHeader + 4]) != 0x004E4942) {
        json_object_put(r);
        throw std::runtime_error("GLB BIN chunk 无效");
    }
    const uint32_t oldBinLength = le32(&in[binChunkHeader]);
    if (binChunkHeader + 8ull + oldBinLength > in.size()) {
        json_object_put(r);
        throw std::runtime_error("GLB BIN chunk 截断");
    }

    Bytes oldBin(in.begin() + binChunkHeader + 8, in.begin() + binChunkHeader + 8 + oldBinLength);
    Bytes newBin = replaceCompressedStream(r, oldBin, targetStream);

    attributes(r, to10);
    cameras(r, to10);
    extLists(r, to10);
    char *out = cJSON_Print(r);
    if (!out) {
        json_object_put(r);
        throw std::runtime_error("JSON 序列化失败");
    }
    std::string js = out;
    cJSON_free(out);
    json_object_put(r);
    while (js.size() % 4)
        js.push_back(' ');
    Bytes b;
    b.insert(b.end(), {'g', 'l', 'T', 'F'});
    putle(b, 2);
    putle(b, 0);
    putle(b, js.size());
    putle(b, 0x4E4F534A);
    b.insert(b.end(), js.begin(), js.end());
    putle(b, newBin.size());
    putle(b, 0x004E4942);
    b.insert(b.end(), newBin.begin(), newBin.end());
    b.insert(b.end(), in.begin() + binChunkHeader + 8 + oldBinLength, in.end());
    uint32_t n = b.size();
    for (int i = 0; i < 4; i++)
        b[8 + i] = n >> (8 * i);
    return b;
}
struct Box {
    size_t p, h, n;
    std::string t;
};
static Box boxAt(const Bytes &b, size_t p) {
    if (p + 8 > b.size())
        throw std::runtime_error("MP4 box 截断");
    uint64_t n = be32(&b[p]);
    size_t h = 8;
    if (n == 1) {
        if (p + 16 > b.size())
            throw std::runtime_error("MP4 large box 截断");
        n = 0;
        for (int i = 0; i < 8; i++)
            n = n << 8 | b[p + 8 + i];
        h = 16;
    }
    if (n < h || p + n > b.size())
        throw std::runtime_error("MP4 box 大小无效");
    return {p, h, (size_t)n, std::string((char *)&b[p + 4], 4)};
}
static Bytes convertMp4(const Bytes &in, bool to10, const Bytes &targetStream) {
    size_t p = 0;
    Box meta{};
    bool found = false;
    while (p < in.size()) {
        auto x = boxAt(in, p);
        if (x.t == "meta") {
            size_t q = p + x.h + 4;
            while (q < p + x.n) {
                auto c = boxAt(in, q);
                if (c.t == "idat") {
                    meta = x;
                    found = true;
                    break;
                }
                q += c.n;
            }
        }
        if (found)
            break;
        p += x.n;
    }
    if (!found)
        throw std::runtime_error("未找到顶层 meta/idat");
    size_t q = meta.p + meta.h + 4;
    Box idat{}, iloc{};
    while (q < meta.p + meta.n) {
        auto c = boxAt(in, q);
        if (c.t == "idat")
            idat = c;
        else if (c.t == "iloc")
            iloc = c;
        q += c.n;
    }
    Bytes glb(in.begin() + idat.p + idat.h, in.begin() + idat.p + idat.n),
        ng = convertGlb(glb, to10, targetStream);
    Bytes out;
    out.insert(out.end(), in.begin(), in.begin() + idat.p);
    putbe(out, uint32_t(ng.size() + 8));
    out.insert(out.end(), {'i', 'd', 'a', 't'});
    out.insert(out.end(), ng.begin(), ng.end());
    out.insert(out.end(), in.begin() + idat.p + idat.n, in.end());
    int64_t delta = int64_t(ng.size()) - int64_t(glb.size());
    setbe(out, meta.p, uint32_t(int64_t(meta.n) + delta));
    if (iloc.n >= 4)
        setbe(out, iloc.p + iloc.n - 4, uint32_t(ng.size()));
    return out;
}
int main(int argc, char **argv) {
    try {
        std::string to, streamPath, inputPath, outputPath;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc)
                throw std::runtime_error("参数缺少取值: " + std::string(argv[i]));
            const std::string option = argv[i], value = argv[i + 1];
            std::string *target = nullptr;
            if (option == "--to")
                target = &to;
            else if (option == "--stream")
                target = &streamPath;
            else if (option == "--input")
                target = &inputPath;
            else if (option == "--output")
                target = &outputPath;
            else
                throw std::runtime_error("未知参数: " + option);
            if (!target->empty())
                throw std::runtime_error("参数重复: " + option);
            *target = value;
        }
        if ((to != "0.4" && to != "1.0") || streamPath.empty() || inputPath.empty() ||
            outputPath.empty()) {
            std::cerr << "用法: " << argv[0]
                      << " --to <0.4|1.0> --stream <目标压缩码流> "
                         "--input <输入.glb|输入.mp4> --output <输出.glb|输出.mp4>\n";
            return 2;
        }
        bool t = to == "1.0";
        Bytes targetStream = readFile(streamPath);
        Bytes in = readFile(inputPath), out;
        if (in.size() >= 4 && std::string((char *)in.data(), 4) == "glTF")
            out = convertGlb(in, t, targetStream);
        else
            out = convertMp4(in, t, targetStream);
        writeFile(outputPath, out);
        std::cout << "转换完成: " << outputPath << " (" << out.size() << " bytes)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 1;
    }
}
