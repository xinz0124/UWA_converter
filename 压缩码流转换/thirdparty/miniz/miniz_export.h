/* miniz_export.h - 静态构建配置 (无 DLL 导出)
 * miniz 2.2.0 引入此头用于 CMake DLL 导出控制.
 * 我们静态链接 miniz, MINIZ_EXPORT 定义为空.
 * 注: 2.2.0 release tag 不含此文件, 故自建. */
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#ifndef MINIZ_EXPORT
#define MINIZ_EXPORT
#endif

#endif /* MINIZ_EXPORT_H */
