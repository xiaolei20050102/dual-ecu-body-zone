# 环境版本记录

把每个工具的版本、安装路径、验证结果写在这里。板子未到时，先完成软件环境检查。

## 基础工具

| 工具 | 版本 | 安装路径 | 验证结果 |
| --- | --- | --- | --- |
| Git | 2.51.0.windows.1 | 待填写 | 已验证 |
| Python | 3.12.5 | 待填写 | 已验证 |
| STM32CubeIDE | 待填写 | 待填写 | 待验证 |
| S32 Design Studio | 待填写 | 待填写 | 待验证 |

## Python 包

| 包 | 版本 | 验证命令 | 验证结果 |
| --- | --- | --- | --- |
| python-can | 4.6.1 | `python -c "import can; print(can.__version__)"` | 已验证 |
| cantools | 42.0.3 | `python -c "import cantools; print(cantools.__version__)"` | 已验证 |

## 配置文件验证

| 文件 | 验证方式 | 结果 |
| --- | --- | --- |
| `config/body_zone.dbc` | `cantools.database.load_file(...)` | 已验证，3 条 CAN 报文 |
