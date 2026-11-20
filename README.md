# Visuals Overlay (OpenGL, Minecraft Java)

Косметический оверлей в виде DLL для Minecraft Java Edition (Windows x64): прицел, эффект клика,
виньетка, свечение краёв, цветовой фильтр, FPS/часы/график кадров, клавиши и CPS,
анимированное меню на ImGui. Меню: `INSERT` (меняется в настройках), `Esc` закрывает.

Minecraft Java рисует через **OpenGL**, поэтому DLL хукает `gdi32!SwapBuffers`.

## Сборка на GitHub
Push в репозиторий, затем вкладка **Actions**. Готовый `visuals.dll` лежит в артефактах запуска.

## Как загрузить
1. Через лаунчер: `System.load("путь\\visuals.dll")` (вызовется `JNI_OnLoad`) или JVM-аргумент
   `-agentpath:путь\visuals.dll` (`Agent_OnLoad`).
2. Обычный `LoadLibrary` в процесс игры (`DllMain`).
Конфиг `visuals.cfg` сохраняется рядом с DLL.

## Ограничение
Оверлей рисует поверх кадра и не видит данные игры (урон, здоровье, FOV). Хитмаркеры, числа урона,
полосы HP и зум требуют интеграции в саму игру, например мода Fabric.
