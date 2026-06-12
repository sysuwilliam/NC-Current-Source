<div align="center">

## 高精度数控恒流源

<hr/>

<p>
  <img src="https://img.shields.io/badge/Team-%E5%81%9A%E7%9A%84%E9%83%BD%E9%98%9F-4b5563?style=for-the-badge" alt="Team 做的都队" />
  <img src="https://img.shields.io/badge/Contest-2026%20%E6%A0%A1%E8%B5%9B%E9%80%89%E9%A2%98%E4%B8%89-2563eb?style=for-the-badge" alt="Contest" />
  <img src="https://img.shields.io/badge/MCU-STM32F103-1f2937?style=for-the-badge" alt="STM32F103" />
  <img src="https://img.shields.io/badge/DAC-DAC8562-1f2937?style=for-the-badge" alt="DAC8562" />
  <img src="https://img.shields.io/badge/Status-Completed-059669?style=for-the-badge" alt="Completed" />
</p>


系统基于 <code>STM32F103 + DAC8562 + OPA197 + 可调 Buck</code>，实现高精度恒流输出、参数显示与保护控制。

</div>

## 项目简介

本项目是 2026 年中山大学电子设计与程序设计校内赛电子设计赛道选题三“高精度数控恒流源”的完整实现。系统以 `STM32F103` 为主控，结合 `DAC8562`、`OPA197`、功率 MOS 管和可调 Buck 电源，完成稳定恒流输出、状态检测、人机交互与保护控制。

项目目标是在不同负载条件下输出高精度、可调节、可显示、可保护的直流恒流。仓库中保留了从方案规划、硬件设计、嵌入式固件、串口屏工程，到测试标定、外壳设计和参考资料的完整内容。

## 主要功能

- 可设定输出电流并稳定恒流输出
- 串口屏显示电流、电压、负载等信息
- 按键与编码器调节参数
- Buck 自动调节输出余量，降低 MOS 发热
- 支持过流、短路、开路保护

## 工作原理

系统采用“硬件恒流内环 + 软件余量外环”的结构：

- `DAC8562 + OPA197 + MOS + Rs` 构成硬件恒流环，直接建立目标电流并负责快速稳流
- `STM32` 采样 `Vsense`、`VOUT+`、`VOUT-`，计算输出电流、负载电压、负载状态和 MOS 压差余量
- `DAC8562` 的另一通道用于控制可调 Buck，动态调节输出电压，为恒流环提供合适工作余量
- 串口屏、按键和编码器负责人机交互，实现参数设置、状态显示和工作控制

控制链路如下：

```text
I_set -> DAC8562 -> OPA197 + MOS + Rs -> I_out
                -> STM32采样与计算 -> Buck调节
```

简要理解就是：

- 模拟电路负责“把电流稳住”
- MCU 负责“把状态算清楚”
- Buck 负责“把供电余量调合适”

## 仓库结构

```text
.
├── README.md                        项目说明
├── 赛题/                            校赛题目原文
├── 规划/                            初期规划、中期报告、收尾总结
├── 代码/
│   ├── 成品/                        最终主固件工程
│   ├── 成品2/                       成品阶段备份工程
│   └── 测试板/                      早期测试板固件工程
├── PCB/
│   └── 测试板/                      PCB 工程文件
├── 串口屏/
│   ├── UI.HMI                       串口屏工程
│   └── 资料/                        串口屏开发手册、例程与协作笔记
├── 测试/
│   ├── 恒流源测试/                  测试规划、测试记录、标定数据与分析图表
│   └── 恒压源测试/                  早期电源与 Buck 联调记录
├── 数据手册/                        DAC、Buck、LDO 等器件手册
├── 外壳/                            3D 外壳模型文件
├── 参考工程/                        参考项目、接口说明、BOM 等资料
└── github协作指南/                  团队协作与 Git 使用说明
```

## 项目状态

项目已完成。仓库当前保存了最终代码、测试数据、串口屏工程、PCB 文件、外壳模型和相关设计资料，可用于展示、复盘和后续迭代参考。
