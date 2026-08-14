我有两个版本的3DGS数据。分别以压缩码流（bin或egsc），将压缩码流封装在glb中的glb文件，以及将glb封装在mp4中的mp4文件。分别为：0.4版本，保存在/Users/xinz/Desktop/ONE/work/project/Huawei/格式转换/version0.4；1.0版本，保存在/Users/xinz/Desktop/ONE/work/project/Huawei/格式转换/version1.0。
其中，1.0和0.4的主要区别：
1. 压缩码流本身的区别，0.4和1.0版本的压缩方法变了（这里我们不需要深究细节）。
2. glb中的字段变了，具体为：
2.1 0.4版本和1.0版本中, attributes下3DGS额外属性名称变了。0.4版本中，为"_OPACITY"，"_SCALE"，"_ROTATION"，"_SPHERICAL_HARMONICS_DEGREE_0_COEFFICIENT_0"，"_SPHERICAL_HARMONICS_DEGREE_1_COEFFICIENT_0"，"_SPHERICAL_HARMONICS_DEGREE_1_COEFFICIENT_1"，"_SPHERICAL_HARMONICS_DEGREE_1_COEFFICIENT_2"，"_SPHERICAL_HARMONICS_DEGREE_2_COEFFICIENT_0"，"_SPHERICAL_HARMONICS_DEGREE_2_COEFFICIENT_1"，"_SPHERICAL_HARMONICS_DEGREE_2_COEFFICIENT_2"，"_SPHERICAL_HARMONICS_DEGREE_2_COEFFICIENT_3"，"_SPHERICAL_HARMONICS_DEGREE_2_COEFFICIENT_4"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_0"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_1"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_2"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_3"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_4"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_5"，"_SPHERICAL_HARMONICS_DEGREE_3_COEFFICIENT_6"；对应的1.0版本，为"KHR_gaussian_splatting:OPACITY"，"KHR_gaussian_splatting:SCALE"，"KHR_gaussian_splatting:ROTATION"，"KHR_gaussian_splatting:SH_DEGREE_0_COEF_0"，"KHR_gaussian_splatting:SH_DEGREE_1_COEF_0"，"KHR_gaussian_splatting:SH_DEGREE_1_COEF_1"，"KHR_gaussian_splatting:SH_DEGREE_1_COEF_2"，"KHR_gaussian_splatting:SH_DEGREE_2_COEF_0"，"KHR_gaussian_splatting:SH_DEGREE_2_COEF_1"，"KHR_gaussian_splatting:SH_DEGREE_2_COEF_2"，"KHR_gaussian_splatting:SH_DEGREE_2_COEF_3"，"KHR_gaussian_splatting:SH_DEGREE_2_COEF_4"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_0"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_1"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_2"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_3"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_4"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_5"，"KHR_gaussian_splatting:SH_DEGREE_3_COEF_6"
2.2 0.4版本中，3DGS压缩码流扩展字段为UWA_primitive_3DGS_compression；而1.0版本中，3DGS压缩码流扩展字段为KHR_gaussian_splatting.UWA_gaussian_splatting_compression_EGSC
2.3 0.4版本中，初始观看相机下面没有UWA_user_camera_label扩展，而1.0版本是有的。
2.4 0.4版本中，观看约束为
```json
"UWA_viewing_parameters": {
          "longitudeRange": [-180, 180],
          "latitudeRange": [60.9053764, 82.7911224],
          "distanceRange": [7.27045202, 10.9056778],
          "gravityCoordinateSystem": [1, 0, 0, 0, 1, 0, 0, 0, 1],
          "target": [0.326565117, -0.160826877, -0.145616904],
          "boundingBoxRange": {
            "x": [-1.90794027, 0.695028186],
            "y": [-0.73137027, 0.621934474],
            "z": [-0.961001873, 0.596473634]
          }
        }
```
1.0版本中，观看约束为
```json
"UWA_viewing_constraints": {
          "modes": [
            {
              "type": "allocentricSixDof",
              "allocentricSixDof": {
                "azimuthRange": [-3.14159274, 3.14159274],
                "polarRange": [1.06299937, 1.44497776],
                "distanceRange": [7.27045202, 10.9056778],
                "target": [0.326565117, -0.160826877, -0.145616904],
                "targetBoundingBox": {
                  "center": [-0.606456041, -0.054717917, -0.182264119],
                  "size": [2.60296845, 1.35330474, 1.55747557]
                }
              }
            }
          ]
        }
```
其中，0.4版本中的角度为角度制，而1.0版本中的角度为弧度制。“longitudeRange”对应“azimuthRange”，"latitudeRange"对应"polarRange"。0.4版本中的boundingBoxRange下xyz均为xyz的范围，而1.0版本下targetBoundingBox，center是xyz方向的中心，size则是在xyz方向的边长。

mp4文件是将glb文件封装到了普通视频mp4文件中，在文件层级的meta box下中的idat中，具体封装方式参考/Users/xinz/Desktop/ONE/work/project/Huawei/《支持六自由度交互的三维图像格式标准》参考工具集v1.0.3/基于MP4封装视频和glTF的参考实现/MP4封装视频和glTF/muxer.py。

写一个c++程序，能够实现0.4版本下的mp4和glb文件与1.0版本的mp4和glb文件的相互转换。