# Open Source Checklist

上传公开仓库前逐项检查。

## 文件完整性

- `hardware/schematic/MPPT-schematic.pdf` 可打开。
- `hardware/fabrication/MPPT-gerber-drill-fabrication.zip` 可解压。
- `hardware/gerber/extracted/` 中包含 Gerber、钻孔和飞针测试文件。
- `docs/images/MPPT-schematic-1.png` 仅作为预览图，PDF 才是正式原理图。

## 授权与发布

- 已选择并添加最终 `LICENSE` 文件，当前为 `CERN-OHL-P-2.0`。
- 已确认硬件许可证适用于 Gerber、原理图和后续 EDA 源工程。
- 已确认文档、图片和发布说明的许可证策略。
- 已确认公开版本不包含个人路径、临时文件或未公开资料。

## 生产资料

- Gerber 层名完整。
- PTH/NPTH 钻孔文件完整。
- 板框层可被 Gerber Viewer 正确识别。
- 飞针测试 JSON 是否需要公开已经确认。
- 如后续有 BOM、CPL/坐标文件、装配图，应放入 `hardware/assembly/`。

## 安全与工程说明

- README 中已标明高压、大电流、光伏、电池安全风险。
- 未宣称未经验证的功率等级。
- 未宣称软件保护可以替代硬件保护。
- 已说明当前资料缺少可编辑 EDA 源文件。

## 建议上传前命令

```powershell
git init
git add .
git status
git commit -m "docs: publish mppt hardware release package"
```
