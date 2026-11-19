# Visuals Overlay

Косметический DX11-оверлей в виде DLL: прицел, хитмаркер, числа урона, частицы,
экранные эффекты, HUD, зум и анимированное меню на ImGui. Рассчитан на **собственные**
приложения и одиночные проекты. Меню открывается клавишей `INSERT` (меняется в настройках).

## Сборка на GitHub
1. Создайте репозиторий и загрузите все файлы этого проекта.
2. Откройте вкладку **Actions**. Сборка запускается при каждом push (или вручную: *Run workflow*).
3. Готовый `visuals.dll` лежит в артефактах запуска (`visuals-dll`).
4. Для релиза создайте тег: `git tag v1.0.0 && git push --tags`. DLL автоматически
   прикрепится к GitHub Release.

## Локальная сборка
```
cmake -S . -B build -A x64
cmake --build build --config Release
```
Нужны Visual Studio 2019+ (C++ workload) и CMake 3.20+. Зависимости (ImGui v1.91.9, MinHook v1.3.3)
скачиваются автоматически.

## Подключение в своём приложении
```cpp
HMODULE h = LoadLibraryA("visuals.dll");
auto onHit  = (void(*)(float,float,float,bool))GetProcAddress(h, "Visuals_OnHit");
auto onKill = (void(*)())GetProcAddress(h, "Visuals_OnKill");
auto stats  = (void(*)(float,float,int,int))GetProcAddress(h, "Visuals_SetStats");
auto yaw    = (void(*)(float))GetProcAddress(h, "Visuals_SetYaw");
auto fovMul = (float(*)())GetProcAddress(h, "Visuals_GetFovMultiplier");
auto show   = (void(*)(bool))GetProcAddress(h, "Visuals_ShowMenu");
```
Приложение должно использовать DirectX 11. Конфиг сохраняется в `visuals.cfg`
рядом с рабочей папкой процесса.
