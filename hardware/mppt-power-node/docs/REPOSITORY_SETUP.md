# Repository Setup

以下命令用于把当前目录初始化为独立开源仓库。

## 本地初始化

在本目录执行：

```powershell
git init
git add .
git status
git commit -m "docs: publish mppt hardware release package"
```

## 推送到 Gitee

先在 Gitee 新建空仓库，然后执行：

```powershell
git remote add origin <你的Gitee仓库地址>
git branch -M main
git push -u origin main
```

## 推送到 GitHub

先在 GitHub 新建空仓库，然后执行：

```powershell
git remote add origin <你的GitHub仓库地址>
git branch -M main
git push -u origin main
```

## 上传前最后确认

必须先处理：

- 阅读 `docs/DESIGN_NOTES.md`，确认公开口径和发布边界。
- 确认 `LICENSE` 为当前预期许可证，当前为 `CERN-OHL-P-2.0`。
- 确认是否公开 `FlyingProbeTesting.json`。
