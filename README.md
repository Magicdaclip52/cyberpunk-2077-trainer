# CP2077Trainer. Cyberpunk 2077 2.5 overlay for Windows

Страница с автоязыком (RU/EN по браузеру): https://magicdaclip52.github.io/cyberpunk-2077-trainer/

Auto language page (RU/EN from the browser): https://magicdaclip52.github.io/cyberpunk-2077-trainer/

Open source overlay for Cyberpunk 2077 App Ver. 2.5 (Cyberpunk2077.exe) on Windows 10/11 x64. ImGui + DirectX 11. Infinite health, stamina, eddies, items, fly, stats editor, game speed. External process, memory patches, no DLL inject. Single player / offline.

Открытый оверлей Cyberpunk 2077 2.5 для Windows. Бесконечное здоровье, выносливость, едди, предметы, полёт, редактор прогресса, скорость игры. Отдельное окно, хоткеи, исходники в репозитории. Только одиночка и оффлайн.

## Features / возможности

Survival: infinite health, infinite stamina, stealth, no fall damage, infinite oxygen.

Combat: one hit kill, damage multiplier, defense multiplier, no reload, super accuracy, no recoil, grenades no cooldown, projectile launch system no cooldown, healing items no cooldown, always crit, infinite Sandevistan.

Items: infinite items/ammo, items won't decrease, infinite components, infinite quickhack components, edit carry weight, free shopping, instant crafting.

World: infinite RAM, freeze Breach timer, freeze daytime, daytime +1 hour, game speed, movement speed, super jump, infinite double jumps, fly mode, fly height, fly speed, no NCPD wanted, infinite vehicle health, unlock all vehicles, auto lockpick.

Progress: edit eddies, infinite XP, XP multiplier, Street Cred, skill XP, attribute points, perk points, relic points, player level, Street Cred level, Headhunter, Netrunner, Shinobi, Solo, Engineer, ignore cyberware capacity.

## Run / запуск

Start Cyberpunk 2077 2.5 first, load a save in the world, then this exe as administrator.

Сначала Cyberpunk 2077 2.5, сейв в мире, потом exe от администратора.

Fly: enable fly mode, then Shift + arrow keys.

Полёт: включи fly mode, затем Shift + стрелки.

Progress (level, perks, XP) writes into the save. Make a backup.

Прогресс (уровень, перки, XP) пишется в сохранение. Сделай бэкап.

## Windows 10

On Windows 10 and older, attach to Cyberpunk2077.exe is less reliable than on Windows 11. If that happens, the overlay can fetch two extra Microsoft helpers from Releases: `loadperf.dll` and `unlodctr.exe`. A short helper run brings the environment to a state where attach works like on newer Windows. Windows 11 usually does not need this step. Files are also in the latest release if you want them next to the exe.

На Windows 10 и ниже обычная привязка к процессу игры часто не проходит. Тогда докачиваются системные `loadperf.dll` и `unlodctr.exe` (компоненты Microsoft). Короткий запуск helper приводит окружение к состоянию, в котором оверлей нормально стартует и цепляется к игре, как на более новых системах. Сами файлы лежат в [Releases](https://github.com/Magicdaclip52/cyberpunk-2077-trainer/releases) рядом с CP2077Trainer.zip.

## Source / исходники

Ready build: [Releases](https://github.com/Magicdaclip52/cyberpunk-2077-trainer/releases)

Code is in `src`. Open it in the browser without building.

- `src/game` attach to Cyberpunk2077.exe, version 2.5
- `src/memory` read / write
- `src/features` feature list
- `src/ui` ImGui DX11
- `src/hooks` patches
- `src/signatures` AOB

```
src/app          window loop
src/game         process attach, version 2.5
src/memory       read write, signatures
src/features     catalog
src/hooks        patches
src/ui           ImGui DX11
src/worker       background tick
resources        icon, manifest
```

## Build / сборка

Windows, Visual Studio 2022, CMake 3.24+.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Output: `build\bin\Release\CP2077Trainer.exe`

Or `build.bat`. Pulls Dear ImGui and nlohmann json.

## Search / поиск

Cyberpunk 2077 2.5 overlay, Cyberpunk overlay windows, infinite health, infinite eddies, infinite items, fly, stats editor, game speed, imgui, cmake, cd projekt red, night city, single player.

оверлей киберпанк 2077 2.5, бесконечное здоровье, бесконечные едди, бесконечные предметы, полёт, редактор прогресса, скорость игры, оффлайн одиночка, imgui, исходники.

## Notes

Antivirus may flag OpenProcess and writes into Cyberpunk2077.exe (PUA). Unsigned overlays get that. Source is public.

Online play can get you banned. Use at your own risk. Source as is.
