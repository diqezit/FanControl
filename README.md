---

# Lenovo Fan Control Utility

A lightweight background utility that forces the fan on certain Lenovo laptops to its highest speed. It's designed to provide maximum, continuous cooling for demanding tasks like gaming or rendering, overcoming the limitations of Lenovo's built-in software.

This project was born from the need to control the fan on a Lenovo Ideapad Z580, where standard tools like SpeedFan failed to work, and has since been engineered to support a wide range of similar models.

### The Problem

Many Lenovo laptops use a proprietary Embedded Controller (EC) interface, making them incompatible with standard fan control software. While Lenovo's own **Energy Management** driver provides a "Dust Removal" feature, it's not a true cooling solution. This feature runs a self-terminating program inside the EC:
-   The fan spins at max speed for about 9 seconds.
-   It then **pauses for 2 seconds**.
-   This cycle repeats for only 2 minutes before stopping completely.

This makes it useless for sustained cooling.

### The Solution: The Proactive Reset Strategy

This utility turns the flawed "Dust Removal" feature into a reliable, continuous max-speed mode. Instead of reactively re-enabling the fan after it stops, this tool works proactively:

1.  It activates the "Dust Removal" program.
2.  Using a high-precision timer, it waits for ~8.8 seconds.
3.  **Just before** the EC's internal timer can trigger the 2-second pause, the utility sends an instantaneous `Normal -> Fast` command sequence.
4.  This action resets the EC's internal program, starting a new 9-second cycle.

The result is continuous, stable, maximum-speed fan operation for as long as the application is running, with the 2-second pause being replaced by a nearly imperceptible, millisecond-long "hiccup".

### Prerequisites
*   The **[Lenovo Energy Management Driver](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)** must be installed. You can check in Device Manager for `Lenovo ACPI-Compliant Virtual Power Controller`.
*   [Download the latest binary from the Releases page.](/releases)

### Usage

This is a simple tray application with no complex settings.
-   **Run `FanControl.exe`**: The application will start in the system tray, and your laptop's fan will immediately spin up to maximum speed.
-   **Exit**: To stop the utility and return to normal, system-controlled fan operation, simply right-click the tray icon and select "Exit".

### Technical Deep Dive: The Inescapable Limitation

During development, an exhaustive reverse-engineering effort was undertaken to find a way to achieve *true* continuous fan control. The analysis led to one fundamental conclusion: **the "hiccup" or "dip" in fan speed during the reset is an unavoidable consequence of the API provided by Lenovo.**

#### The Problem is the API, Not the Code

We cannot eliminate the fan speed dip because we are not directly controlling the fan. The API exposed by Lenovo through the `EnergyDrv` driver does not provide a function to "set fan speed to X%". Instead, it only allows us to trigger a hard-coded, self-contained "Dust Removal" program inside the Embedded Controller (EC).

#### All Paths to an "Ideal" Solution Are Blocked:

1.  **No Alternative ACPI Methods Exist:** A full DSDT dump analysis confirms that there are no other exposed ACPI methods for direct fan control.
2.  **Faking EC Input Data is Impossible (with this driver):** The driver's API does not provide a generic function to write to arbitrary EC memory addresses (e.g., to fake temperature sensor data).
3.  **Modifying EC Firmware is Unfeasible and Dangerous:** This would require expert-level skills and carries a near-certain risk of bricking the laptop.

### Conclusion: The Proactive Reset is the Pinnacle of What's Possible

Given these hard limitations, the **"proactive reset" strategy is the absolute maximum that can be safely and reliably achieved under the circumstances.**

This utility represents the most optimized implementation of this strategy. It cannot change the physical limitations of the API, but it minimizes the resulting "dip" to the shortest possible duration.

### Disclaimer

This software is provided as is. Use at your own risk. No warranty is provided.

---
---

## Утилита для Управления Вентилятором Lenovo (Русская версия)

Это легкая фоновая утилита, которая принудительно включает вентилятор на некоторых ноутбуках Lenovo на максимальную скорость. Она предназначена для обеспечения максимального, непрерывного охлаждения при выполнении требовательных задач, таких как игры или рендеринг, преодолевая ограничения встроенного программного обеспечения Lenovo.

Проект возник из-за необходимости управлять вентилятором на Lenovo Ideapad Z580, где стандартные инструменты, такие как SpeedFan, не работали, и с тех пор был доработан для поддержки широкого спектра аналогичных моделей.

### Проблема

Многие ноутбуки Lenovo используют проприетарный интерфейс Встроенного Контроллера (EC) для управления вентилятором, что делает их несовместимыми со стандартными программами. Хотя собственный драйвер Lenovo **Energy Management** предоставляет функцию «Удаление пыли», это не является настоящим решением для охлаждения. Эта функция запускает самозавершающуюся программу внутри EC:
-   Вентилятор вращается на максимальной скорости около 9 секунд.
-   Затем он **приостанавливается на 2 секунды**.
-   Этот цикл повторяется всего 2 минуты, после чего полностью прекращается.

Это делает функцию бесполезной для длительного охлаждения.

### Решение: Стратегия Проактивного Сброса

Эта утилита превращает несовершенную функцию «Удаление пыли» в надежный, непрерывный режим максимальной скорости. Вместо того чтобы реактивно включать вентилятор после его остановки, этот инструмент работает проактивно:

1.  Он активирует программу «Удаление пыли».
2.  Используя высокоточный таймер, он ждет примерно 8.8 секунд.
3.  **Прямо перед тем**, как внутренний таймер EC сможет запустить 2-секундную паузу, утилита отправляет мгновенную последовательность команд: `Установить Нормальный Режим` -> `Установить Быстрый Режим`.
4.  Это действие перезапускает внутреннюю программу EC, начиная новый 9-секундный цикл.

Результатом является непрерывная, стабильная работа вентилятора на максимальной скорости, пока приложение запущено, а 2-секундная пауза заменяется почти незаметным «провалом» длительностью в миллисекунды.

### Требования
*   Должен быть установлен **[Драйвер Lenovo Energy Management](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)**. Вы можете проверить его наличие в Диспетчере устройств в разделе «Системные устройства» по названию `Lenovo ACPI-Compliant Virtual Power Controller`.
*   [Скачайте последнюю версию со страницы Releases.](/releases)

### Использование

Это простое приложение для системного трея без сложных настроек.
-   **Запустите `FanControl.exe`**: Приложение запустится в системном трее, и вентилятор вашего ноутбука немедленно начнет вращаться на максимальной скорости.
-   **Выход**: Чтобы остановить утилиту и вернуться к обычному, управляемому системой режиму, просто щелкните правой кнопкой мыши по значку в трее и выберите "Exit".

### Глубокий Технический Анализ: Непреодолимое Ограничение

В ходе разработки была предпринята исчерпывающая попытка обратной разработки для поиска способа достижения *истинного* непрерывного управления вентилятором. Это включало полный анализ таблиц DSDT (ACPI) и поведения драйвера Lenovo `EnergyDrv`.

Анализ привел к одному фундаментальному выводу: **«провал» или «падение» скорости вентилятора во время сброса является неизбежным следствием API, предоставляемого Lenovo.**

#### Проблема в API, а не в коде

Мы не можем устранить падение скорости, потому что мы не управляем вентилятором напрямую. API, предоставляемый Lenovo через драйвер `EnergyDrv`, не имеет функции «установить скорость вентилятора на X%». Вместо этого он позволяет лишь запустить жестко запрограммированную программу «Удаление пыли» внутри Встроенного Контроллера (EC).

#### Все пути к «идеальному» решению заблокированы:

1.  **Отсутствие альтернативных методов ACPI:** Полный анализ дампа DSDT подтверждает, что других открытых методов ACPI для прямого управления вентилятором не существует.
2.  **Невозможность подмены входных данных для EC (с этим драйвером):** API драйвера не предоставляет универсальной функции для записи в произвольные адреса памяти EC (например, для подмены данных с датчика температуры).
3.  **Изменение прошивки EC нецелесообразно и опасно:** Это потребовало бы экспертных навыков и несет почти стопроцентный риск превратить ноутбук в «кирпич».

### Заключение: Проактивный сброс — это вершина возможного

Учитывая эти жесткие ограничения, **стратегия «проактивного сброса» — это абсолютный максимум, которого можно достичь безопасно и надежно в данных обстоятельствах.**

Эта утилита представляет собой наиболее оптимизированную реализацию этой стратегии. Она не может изменить физические ограничения API, но минимизирует их последствия до кратчайшего возможного времени.

### Отказ от ответственности

Программное обеспечение предоставляется «как есть». Используйте на свой страх и риск. Гарантии не предоставляются.

---
---

## 联想风扇控制工具 (中文版)

这是一款轻量级的后台工具，可以强制某些联想笔记本电脑的风扇以最高速度运行。它旨在为游戏或渲染等高负载任务提供最大化、持续的散热，突破联想内置软件的限制。

该项目的诞生源于控制联想Ideapad Z580风扇的需求，因为像SpeedFan这样的标准工具无法在该设备上工作。此后，项目经过优化，已能够支持更多类似的型号。

### 问题所在

许多联想笔记本电脑使用专有的嵌入式控制器（EC）接口来管理风扇，这使得它们与标准风扇控制软件不兼容。虽然联想官方的**能源管理**驱动程序提供了“除尘”功能，但这并非真正的散热解决方案。该功能会在EC内部运行一个自终止程序：
-   风扇以最高速度旋转约9秒。
-   然后**暂停2秒**。
-   此循环仅重复2分钟，然后完全停止。

这使得它在需要长时间持续散热时毫无用处。

### 解决方案：主动重置策略

本工具将这个有缺陷的“除尘”功能转变为可靠、持续的最高速模式。它并非在风扇停止后被动地重新启动，而是主动出击：

1.  它首先激活“除尘”程序。
2.  使用高精度计时器，它会等待约8.8秒。
3.  在EC的内部计时器即将触发2秒暂停**之前**，该工具会发送一个瞬时的“正常模式” -> “高速模式”命令序列。
4.  此操作会重置EC的内部程序，开始一个新的9秒循环。

最终结果是，只要程序在运行，风扇就能以持续、稳定的最高速度工作，而2秒的暂停被几乎无法察觉的、毫秒级的“顿挫”所取代。

### 使用前提
*   必须安装**[联想能源管理驱动](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)**。您可以在设备管理器的“系统设备”下检查是否存在 `Lenovo ACPI-Compliant Virtual Power Controller`。
*   [从Releases页面下载最新的可执行文件。](/releases)

### 使用方法

这是一个简单的系统托盘程序，没有复杂的设置。
-   **运行 `FanControl.exe`**: 程序将在系统托盘中启动，您的笔记本风扇将立即提升至最高速。
-   **退出**: 要停止程序并恢复到系统自动控制模式，只需右键单击托盘图标并选择“退出”。

### 深入技术分析：无法规避的限制

在开发过程中，我们进行了详尽的逆向工程，试图找到实现*真正*持续风扇控制的方法。这包括对DSDT (ACPI) 表和联想 `EnergyDrv` 驱动程序的全面分析。

分析得出了一个根本性的结论：**风扇在重置期间的速度“顿挫”或“下降”，是联想提供的API所带来的无法避免的后果。**

#### 问题在于API，而非代码

我们无法消除速度下降，因为我们并非在直接控制风扇。联想通过 `EnergyDrv` 驱动程序暴露的API并未提供“将风扇速度设置为X%”的函数。它只允许我们触发一个在嵌入式控制器（EC）中硬编码的“除尘”程序。

#### 所有通往“理想”解决方案的道路都被堵死了：

1.  **不存在其他ACPI方法：** 对DSDT的完整分析证实，没有其他可用于直接控制风扇的公开ACPI方法。
2.  **无法伪造EC输入数据（使用此驱动）：** 此驱动程序的API不提供向EC任意内存地址写入数据的通用功能（例如，伪造温度传感器数据）。
3.  **修改EC固件不可行且极其危险：** 这需要专家级的技能，并且在操作中稍有不慎，几乎必然会导致笔记本“变砖”。

### 结论：主动重置已是可能范围内的巅峰之作

鉴于这些硬性限制，**“主动重置”策略是在当前情况下能够安全、可靠地实现的最大目标。**

本工具是这一策略的最佳优化实现。它无法改变API的物理限制，但通过使用持久化的驱动句柄和瞬时的“正常->高速”命令序列，它将由此产生的速度“顿挫”时间降至了最短。

### 免责声明

本软件按“原样”提供。使用风险自负。不提供任何保证。
