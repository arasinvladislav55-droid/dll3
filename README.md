# UniversalVisuals

Косметический OpenGL-оверлей для Minecraft Java Edition (1.19–1.21+):
современный ClickGUI на Dear ImGui, анимированный HUD, «китайская шляпа»,
кольца прыжка, партиклы, туман, время суток, плавный зум и настройка вьюмодели.

Оверлей рисуется поверх кадра через хук `wglSwapBuffers` (MinHook) и **не
влияет на геймплей**: никаких боевых функций, только визуал.

> Используйте только там, где это разрешено правилами сервера. Проект не
> обходит античиты и не взаимодействует с сетью.

---

## Возможности

### ClickGUI
- Тёмная стеклянная панель, акцентные градиенты, плавное открытие/закрытие.
- Пять вкладок: **HUD**, **World Visuals**, **Player Cosmetics**, **Animations**, **Config**.
- Кастомные тумблеры, слайдеры с анимацией, чипы времени суток, выбор цвета.
- Перетаскивание окна за шапку, кастомный скроллбар, тосты, живой статус.
- Горячие клавиши: `Right Shift` — меню, `Esc` — закрыть, `1`–`5` — вкладки.

### HUD (перетаскивается при открытом меню)
- Watermark с градиентом и бликом.
- FPS + живой график за 9 секунд, CPS (ЛКМ/ПКМ).
- Координаты XYZ + пересчёт в Нижний мир.
- Броня с полосками прочности.
- Keystrokes WASD / Space / Shift с пружинной анимацией нажатия.

### World Visuals
- Клиентское время суток (рассвет/полдень/закат/полночь + слайдер).
- Туман: цвет и плотность.
- Экранный тинт с виньеткой.
- Плавный зум (OptiFine-style) с настройкой FOV и сглаживания.

### Player Cosmetics
- China Hat: объёмный конус с вращающимся бликом, наклоном и парой цветов.
- Jump Rings: расширяющиеся кольца при прыжке.
- Particle Trail: мягкие искры вокруг ног.

### Animations
- Пружинная анимация клавиш, плавный замах, смещение/масштаб руки.

### Config
- Масштаб интерфейса и прозрачность меню, скорость анимации.
- Пресеты акцента, свой цвет, «радужный» акцент.
- Переназначение всех клавиш, автосохранение, сброс настроек.
- Конфиг лежит рядом с DLL: `UniversalVisuals.cfg`.

---

## Сборка локально (Windows + MSVC)

Нужны Visual Studio 2022/2026 с C++ и CMake ≥ 3.20.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Готовый файл: `build/Release/UniversalVisuals.dll`.
При первой сборке CMake скачает ImGui `v1.91.9` и MinHook `v1.3.3`
(нужен интернет).

### Сборка на GitHub Actions

1. Загрузите содержимое этой папки в репозиторий (файлов меньше 100).
2. Откройте вкладку **Actions** → workflow **Build DLL** → **Run workflow**.
3. После сборки скачайте артефакт **UniversalVisuals-dll**.
4. Тег вида `v1.0` дополнительно публикует DLL в **Releases**.

Сборка идёт во временную папку раннера, поэтому файл или папка `build`
в репозитории не мешает конфигурации.

---

## Запуск

1. Соберите `UniversalVisuals.dll`.
2. Инжектните её в процесс Minecraft (любой инжектор, поддерживающий x64 DLL).
3. Нажмите `Right Shift` в игре — откроется меню.

Лог пишется в `%TEMP%\UniversalVisuals.log` — там видно, когда найден хук и
инициализирован ImGui.

### Без Minecraft

HUD и косметика используют данные игрового моста. Пока мост не подключён,
включите **Config → Player Cosmetics → Demo preview**: появятся синтетические
якоря игрока, и все эффекты можно настроить прямо в главном меню игры.

---

## Интеграция с вашей версией Minecraft

Вся игровая специфика изолирована в `src/core/bridge.h`. Оверлей сам ничего не
читает из памяти и не вызывает JNI — вы наполняете мост из своего мода/хука.

### Данные игрока (раз в тик)

```cpp
uv::bridge::PlayerState state;
state.valid = true;
state.x = player.getX();
state.y = player.getY();
state.z = player.getZ();
state.yaw = player.getYRot();
state.pitch = player.getXRot();
state.onGround = player.onGround();
state.speed = /* горизонтальная скорость, блоков/с */;
state.ping = /* ping */;

// Проекция точки над головой и под ногами в экранные координаты:
state.head.valid = true;
state.head.screen = projectToScreen(player.getX(), player.getY() + 1.9, player.getZ());
state.head.radius = /* радиус головы в пикселях */;

state.feet.valid = true;
state.feet.screen = projectToScreen(player.getX(), player.getY() + 0.1, player.getZ());
state.feet.radius = /* радиус в пикселях */;

// Броня: helmet, chestplate, leggings, boots
state.armor[0].present = true;
strcpy_s(state.armor[0].name, "Netherite Helmet");
state.armor[0].durability = 0.82f;

uv::bridge::SubmitPlayer(state);
```

### Мир

```cpp
uv::bridge::WorldState world;
world.valid = true;
world.timeOfDay = (int)level.getDayTime() % 24000;
uv::bridge::SubmitWorld(world);
```

### Управление игрой

В `src/core/bridge.cpp` замените заглушки и выставьте флаги возможностей:

| Функция | Что сделать | Флаг |
| --- | --- | --- |
| `SetWorldTime(int)` | записать время в клиентский Level | `g_hasTime = true` |
| `SetFog(r,g,b,d)` | подменить цвет/плотность тумана | `g_hasFog = true` |
| `SetFov(float)` | выставить FOV камеры | `g_hasFov = true` |
| `ApplyViewModel(...)` | сдвинуть/масштабировать руку | `g_hasViewmodel = true` |

Пока флаг `false`, соответствующая настройка показывает подсказку в меню и не
делает ничего — игра не ломается.

---

## Структура

```
CMakeLists.txt
.github/workflows/build.yml
src/
  dllmain.cpp              точка входа DLL, JNI_OnLoad, Agent_OnLoad
  core/
    config.h/.cpp          конфиг (POD, версия, сохранение рядом с DLL)
    overlay.h/.cpp         кадр, меню, тосты, FPS/CPS, захват клавиш
    hook.h/.cpp            wglSwapBuffers + SwapBuffers, WndProc, виртуальный курсор
    bridge.h/.cpp          единственная точка интеграции с Minecraft
  gfx/
    renderer.h/.cpp        ImGui-контекст, шрифты, DPI, стиль, draw-хелперы
    theme.h/.cpp           акцент, пресеты, градиенты
    gui.h/.cpp             ClickGUI (сайдбар, вкладки, футер)
    widgets.h/.cpp         тумблеры, слайдеры, чипы, кейбинды, цвет
    hud.h/.cpp             перетаскиваемый HUD
    visuals.h/.cpp         шляпа, кольца, партиклы, тинт, зум
  util/
    anim.h                 математика, easing, пружины
    log.h/.cpp             лог в %TEMP%\UniversalVisuals.log
```

---

## Технические заметки

- Хук ставится и на `wglSwapBuffers`, и на GDI `SwapBuffers`; защита от
  двойного рендера — `thread_local` глубина.
- Инициализация происходит при первом SwapBuffers окна класса `GLFW*`/`LWJGL*`,
  то есть уже с текущим GL-контекстом игры.
- Кадр рисуется в FBO `0`, после чего прежний framebuffer восстанавливается.
- Шрифты грузятся в 2× размере и масштабируются обратно — текст остаётся
  чётким при любом UI scale. Если рядом с DLL положить `Inter-*.ttf`,
  используются они, иначе Segoe UI.
- Пока меню открыто, оверлей забирает мышь/клавиатуру и поднимает счётчик
  `ShowCursor`, чтобы курсор был виден в игре; при закрытии всё возвращается.
- Курсор в игре скрыт и зажат, поэтому позиция накапливается из `WM_INPUT`
  (raw deltas), а не из системного курсора.
- HUD/эффекты автоматически скрываются вне мира (титульный экран, пауза),
  если включена настройка **Hide outside a world**.

## Лицензия

Проект предоставлен как есть, для личного использования и обучения.
ImGui (MIT) и MinHook (BSD-2-Clause) сохраняют свои лицензии.
