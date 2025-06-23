Of course. Here is the professionally translated and adapted English version of the README file.

---

# Lenovo Fan Control — The Ultimate Cooling Solution

A lightweight utility that forces the fan on Lenovo laptops to run at a **constant maximum speed**. Designed for enthusiasts, gamers, and professionals who require stable, high-performance cooling under heavy loads—something unattainable with standard Lenovo software.

This project was born out of the need to tame the cooling system of a Lenovo Ideapad Z580 and has since evolved into a universal solution for many models suffering from the same core issue.

## The Problem: The Illusion of Control

Many Lenovo laptops use a proprietary Embedded Controller (EC) that is inaccessible to standard utilities like SpeedFan. The official **Energy Management** driver offers a "Dust Removal" feature, but this is a marketing gimmick, not a real cooling tool. When activated, it runs a hard-coded, self-terminating program on the EC:
-   The fan spins at max speed for ~9 seconds.
-   It then **pauses for 2 full seconds.**
-   This cycle repeats for only 2 minutes before shutting down completely.

For sustained heavy tasks, this is utterly useless.

## The Solution: The Proactive Reset Strategy

This utility transforms the flawed "Dust Removal" feature into a reliable, continuous turbo mode. Instead of waiting for the fan to stop, we act proactively:

1.  The max speed mode is activated.
2.  A high-precision timer counts down for ~8.8 seconds.
3.  Milliseconds **before** the EC's internal schedule can trigger the 2-second pause, the utility sends an instantaneous reset command (`Normal` -> `Fast`).
4.  This action resets the controller's internal timer, starting a new 9-second cycle.

The result is **stable, uninterrupted maximum-speed fan operation**, with the annoying 2-second pause replaced by an imperceptible, millisecond-long "hiccup" that has no impact on overall cooling performance.

---

## Quick Start

### Prerequisites
*   The **[Lenovo Energy Management Driver](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)** must be installed. Ensure that `Lenovo ACPI-Compliant Virtual Power Controller` is present under "System devices" in Device Manager.
*   [Download the latest release from the Releases page.](/releases)

### Usage
Simplicity is key.
-   **Run `FanControl.exe`**: The fan will immediately spin up to maximum speed. An icon will appear in the system tray.
-   **Exit the program**: To return control to the system, simply right-click the tray icon and select "Exit".

---

## Behind the Scenes: A Technical Deep Dive

Why can't we just turn it on and forget it? Because Lenovo doesn't give us that option. What this utility does is not a compromise; it's **the pinnacle of what is technically achievable** with the provided API.

This conclusion is based on an exhaustive reverse-engineering effort of the `AcpiVpc.sys` driver.

### Driver Architecture: A Middle-Man, Not an Engineer

Our investigation revealed that the `AcpiVpc.sys` driver does not manage hardware directly. It is a **filter driver**, a middle-man whose job is to accept a command from an application and pass it down the system stack to the real executor (most likely the system's `acpi.sys`).

![image](https://github.com/user-attachments/assets/acb7cde5-7275-4cda-9925-5d3baf6d8054)
*Decompiled initialization function in Ghidra, demonstrating the creation of a virtual device (`IoCreateDevice`) and its attachment to the device stack (`IoAttachDeviceToDeviceStack`).*

`AcpiVpc.sys` creates a virtual device `\\.\EnergyDrv` so our applications can "see" it, but it does not know or process the control commands itself. It merely ensures their transit.

### Why the "Hiccup" is Unavoidable

Because **the problem is the API, not our code.** We cannot eliminate the brief dip in RPM because the API provided by Lenovo does not offer a function like "set fan speed to 100%". The only command available to us is "run the dust removal program." We can't change that program; we can only restart it in time.

#### All Paths to an Ideal Solution Are Blocked:

1.  **No Alternative ACPI Methods:** A full analysis of the laptop's DSDT tables confirmed that no other methods for direct fan control are exposed.
2.  **Spoofing Data is Impossible:** The driver's API does not allow writing to arbitrary EC memory addresses, which would be necessary to, for example, fake temperature sensor data.
3.  **Modifying EC Firmware is a Path to a Brick:** Any attempt to alter the Embedded Controller's firmware is almost guaranteed to permanently damage the laptop.

### Conclusion
The **"proactive reset" strategy** is the most effective and safest method that could be developed within the existing constraints. This utility represents its most optimized implementation.

---
### Disclaimer
This software is provided "as is." Use at your own risk. The author is not responsible for any potential consequences.

---
# Lenovo Fan Control — Ультимативное Решение для Охлаждения

Легковесная утилита, которая заставляет вентилятор на ноутбуках Lenovo работать на **постоянной максимальной скорости**. Создана для энтузиастов, геймеров и профессионалов, которым требуется стабильное охлаждение под высокими нагрузками, недостижимое со стандартным ПО от Lenovo.

Проект родился из необходимости обуздать систему охлаждения Lenovo Ideapad Z580 и с тех пор превратился в универсальное решение для множества моделей, страдающих от одной и той же проблемы.

## Проблема: Иллюзия контроля

Многие ноутбуки Lenovo используют проприетарный Встроенный Контроллер (EC), закрытый для стандартных утилит вроде SpeedFan. Официальный драйвер **Energy Management** предлагает функцию «Очистка от пыли», но это лишь маркетинговый трюк, а не реальный инструмент охлаждения. При ее активации в контроллере запускается жестко запрограммированный цикл:
-   ~9 секунд работы на пределе.
-   **2 секунды полной остановки.**
-   Повторение цикла в течение всего 2 минут, после чего — полное отключение.

Для продолжительной работы под нагрузкой это абсолютно бесполезно.

## Решение: Стратегия «Опережающего Сброса»

Эта утилита превращает ущербную функцию «Очистки от пыли» в надежный, непрерывный турбо-режим. Вместо того чтобы ждать, пока вентилятор остановится, мы действуем на опережение:

1.  Активируется режим максимальной скорости.
2.  Высокоточный таймер отсчитывает ~8.8 секунд.
3.  За доли секунды **до того**, как контроллер по своему расписанию уйдет на 2-секундную паузу, утилита отправляет мгновенную команду сброса (`Normal` -> `Fast`).
4.  Это действие обнуляет внутренний таймер контроллера, запуская новый 9-секундный цикл.

В результате мы получаем **стабильную, непрерывную работу вентилятора на максимуме**, а раздражающая двухсекундная пауза превращается в едва уловимый, миллисекундный "провал", который не влияет на общую эффективность охлаждения.

---

## Быстрый старт

### Требования
*   Установленный **[Драйвер Lenovo Energy Management](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)**. Убедитесь, что в "Диспетчере устройств" -> "Системные устройства" есть `Lenovo ACPI-Compliant Virtual Power Controller`.
*   [Скачайте последнюю версию со страницы Releases.](/releases)

### Использование
Простота — ключ к успеху.
-   **Запустите `FanControl.exe`**: Вентилятор немедленно раскрутится до предела. Иконка появится в системном трее.
-   **Закройте программу**: Чтобы вернуть управление системе, просто кликните правой кнопкой по иконке в трее и выберите "Выход".

---

## За кулисами: Глубокий технический анализ

Почему нельзя просто сделать "вкл" и забыть? Потому что Lenovo не дает нам такой возможности. То, что делает эта утилита — не компромисс, а **вершина того, что в принципе возможно достичь** с помощью предоставленного API.

Этот вывод основан на исчерпывающем реверс-инжиниринге драйвера `AcpiVpc.sys`, который мы провели.

### Архитектура драйвера: Не инженер, а посредник

Наше исследование показало, что драйвер `AcpiVpc.sys` не управляет оборудованием напрямую. Это **фильтрующий драйвер-посредник**, чья работа — принять команду от приложения и передать ее дальше вниз по системному стеку, настоящему исполнителю (скорее всего, системному `acpi.sys`).

![image](https://github.com/user-attachments/assets/acb7cde5-7275-4cda-9925-5d3baf6d8054)
*Декомпилированный код функции инициализации в Ghidra, демонстрирующий создание виртуального устройства (`IoCreateDevice`) и его присоединение к стеку (`IoAttachDeviceToDeviceStack`).*

`AcpiVpc.sys` создает виртуальное устройство `\\.\EnergyDrv`, чтобы наши приложения могли его "видеть", но сам он не знает и не обрабатывает команды управления. Он просто обеспечивает их транзит.

### Почему "провал" в скорости неизбежен?

Потому что **проблема в самом API, а не в нашем коде.** Мы не можем устранить короткое падение оборотов, так как API от Lenovo не предоставляет функции "установить скорость на 100%". Единственная доступная нам команда — "запустить программу очистки от пыли". Мы не можем изменить эту программу, мы можем лишь вовремя ее перезапускать.

#### Все пути к идеальному решению оказались заблокированы:

1.  **Нет альтернативных методов ACPI:** Полный анализ DSDT-таблиц ноутбука подтвердил — других методов для прямого управления вентилятором просто не существует.
2.  **Невозможность подмены данных:** API драйвера не позволяет записывать данные в произвольные адреса контроллера, чтобы, например, обмануть датчик температуры.
3.  **Модификация прошивки EC — путь к "кирпичу":** Любая попытка изменить прошивку Встроенного Контроллера почти гарантированно приведет к необратимой поломке ноутбука.

### Заключение
Стратегия **"опережающего сброса"** — это самый эффективный и безопасный метод, который только можно было разработать в рамках существующих ограничений. Данная утилита является его наиболее оптимизированной реализацией.

---
### Отказ от ответственности
Программное обеспечение предоставляется «как есть». Используйте на свой страх и риск. Автор не несет ответственности за возможные последствия.
