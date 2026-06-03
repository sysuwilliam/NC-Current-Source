# 校准与可选 PI 调控公式汇总

> 来源：`测试数据01分析.md`、`测试数据02分析.md`。
> 若 01、02 公式不同，以 `测试数据02分析.md` 为准。

## 0. 软件实现优先级建议

建议按以下顺序实现：

```text
1. 电流前馈校准：I_set -> DAC2_cmd
2. 电流显示/反馈校准：I_serial -> I_disp / I_feedback
3. 显示滤波与 I_fast 快速保护量
4. VOUT+ / VOUT- 校准与 Rload 显示
5. 过流、短路、开路保护逻辑
6. BUCK 前馈公式：VOUTP_target -> DAC1_cmd
7. 可选 PI 慢速修正：只修正剩余小误差
```

## 1. DAC2命令值 -> 实际输出电流

```text
I_meas_mA = 0.498188 * DAC2_cmd_mV + 1.368386
```

用途：根据 DAC2 命令值估算实际输出电流。

反算用于电流设定前馈：

```text
DAC2_cmd_mV = (I_set_mA - 1.368386) / 0.498188
```

用途：作为电流设定的前馈校准公式，先给 DAC2 一个接近目标电流的命令值。

## 2. DAC2实测电压 -> 实际输出电流

```text
I_meas_mA = 0.497253 * DAC2_meas_mV - 0.029041
```

用途：验证 DAC2 实际输出电压与恒流环输出电流是否一致。`DAC2_meas` 是万用表测 DAC_CH2 测试点得到的实测电压。

理论公式：

```text
Vref_i = VDAC2 * 49.9k / (49.9k + 330)
Iout = Vref_i / Rsense
```

若 `Rsense = 2Ω`：

```text
Iout_mA = 0.496715 * DAC2_meas_mV
```

用途：作为 DAC2 恒流链路的理论基准。此项主要用于排故，不是固件主控制公式；固件主控制优先使用第 1 节的 `DAC2_cmd -> I_meas`。

## 3. 串口/ADC电流显示校准

```text
I_feedback_mA = 0.980222 * I_serial_mA + 0.023329
```

用途：把 MCU 原始换算电流校准为更接近电流表的反馈电流。该值可作为：

```text
I_disp：最终显示电流
I_feedback：可选 PI 慢速修正的反馈量
```

建议后续代码里统一保留 `I_disp` 作为给用户看的显示值；若用于 PI，可在内部等价使用 `I_feedback`。

## 4. VOUT+ 显示校准

```text
VOUTP_cal = 0.985864 * VOUTP_serial + 0.001825
```

用途：校准 VOUT+ 显示值，用于后续计算负载电压、负载电阻和 BUCK 余量。

## 5. VOUT- 显示校准

```text
VOUTN_cal = 0.998639 * VOUTN_serial - 0.026753
```

用途：校准 VOUT- 显示值，用于计算 MOS 压差和负载电压。

## 6. 负载电压

```text
Vload = VOUTP_cal - VOUTN_cal
```

用途：计算负载两端真实电压。

## 7. MOS压差余量

```text
Vsense_cal_V = I_feedback * Rsense
VMOS = VOUTN_cal - Vsense_cal_V
```

用途：判断恒流环是否有足够压差余量。`VMOS` 太低时，恒流环可能进入饱和。

建议初步判断：

```text
VMOS < 0.5V：余量不足，建议提高 BUCK 或报警
VMOS 约 1.0V ~ 1.6V：较合适
VMOS 太大：MOS 功耗增加，需要降低 BUCK
```

## 8. 负载电阻估算

```text
Rload_est = Vload / I_feedback_A
```

用途：估算当前负载阻值，用于计算 BUCK 需要提供的输出电压。

注意低电流保护：

```text
I < 5mA：Rload 显示 --
5~10mA：可显示但强滤波
I ≥ 10mA：正常显示
```

原因：低电流时分母太小，轻微 ADC 波动会导致 Rload 显示明显跳变。

## 9. BUCK输出目标电压

```text
VOUTP_target = I_set_A * Rload_est + I_set_A * Rsense + VMOS_margin
```

当前建议：

```text
Rsense = 2Ω
VMOS_margin = 1.2~1.5V，可根据温升和恒流稳定性调整
```

用途：根据目标电流、负载阻值和 MOS 余量，计算 DAC1/BUCK 应该给出的 VOUT+。


## 10. DAC1命令值反算

测试拟合关系为：

```text
VOUT+ = 10.012879 * DAC1_actual_V - 1.797554
```

软件实际控制的是 `DAC1_cmd`，不是万用表测得的 `DAC1_actual`。由于当前 DAC1 误差较小，可先近似认为：

```text
DAC1_actual ≈ DAC1_cmd
```

反算：

```text
DAC1_cmd_V = (VOUTP_target + 1.797554) / 10.012879
```

用途：根据目标 BUCK 输出电压，反算 DAC1 应输出的控制电压。

建议加限幅：

```text
DAC1_cmd_V 限制在实际可用范围内，例如 0V ~ 3.3V
VOUT+_target 也应限制在 BUCK 和输入电源允许范围内（2V~28V）
```

## 11. ADC原始电压换算

```text
Vadc_mV = ADC_raw * 3300 / 4095
```

VOUT 分压为 `330k/33k`，比例约为 11：

```text
VOUT_mV = Vadc_mV * 11
```

用途：把 ADC 原始值换算为 VOUT+ / VOUT- 电压。

注意：这是理想换算，最终用于显示和计算时应再进入第 4、5 节的 VOUT 校准公式。

## 12. Vsense电流原始换算

```text
I_serial_mA = Vsense_mV / Rsense_ohm
```

若 `Rsense = 2Ω`：

```text
I_serial_mA = Vsense_mV / 2
```

用途：由采样电阻电压换算 MCU 原始电流显示值，再进入第 3 节电流显示校准公式。

建议区分：

```text
I_serial：原始换算电流
I_disp：校准 + 滤波后的显示电流
I_fast：短平均、少滤波后的保护用电流
```

## 13. PI 慢速修正核心公式

前馈：

```text
DAC2_ff_mV = (I_set_mA - 1.368386) / 0.498188
```

反馈：

```text
I_feedback_mA = 0.980222 * I_serial_mA + 0.023329
```

误差：

```text
error_mA = I_set_mA - I_feedback_mA
```

最终 DAC2 输出：

```text
DAC2_cmd_mV = DAC2_ff_mV + PI_trim_mV
```

其中：

```text
PI_trim_mV = PI(error_mA)
```

注意：

```text
1. PI_trim 的单位必须是 mV。
2. 建议先不加 D 项，避免放大 ADC 噪声。
3. PI 更新周期建议 100ms ~ 500ms。
4. PI_trim 必须限幅，例如 -10mV ~ +10mV。
5. 调试初期可直接令 PI_trim = 0，只使用前馈校准。
```

用途：用标定前馈快速接近目标，再由 PI 慢速修正剩余误差。此项是可选增强，不是当前第一优先级。

## 14. 电流纹波换算

```text
I_ripple_pp = Vpp / Rsense
```

若 `Rsense = 2Ω`：

```text
I_ripple_pp_mA = Vpp_mV / 2
```

用途：由采样电阻上的纹波电压换算输出电流纹波。

注意：普通示波器探头长地线容易引入开关噪声，重测纹波时应使用短地弹簧、AC 耦合、20MHz 限带，并先测噪声底。
