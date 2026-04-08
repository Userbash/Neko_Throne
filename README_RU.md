# Throne (ранее — Nekoray)

Кроссплатформенный GUI-клиент для управления прокси, работающий на базе [Sing-box](https://github.com/SagerNet/sing-box). Поддерживает Windows 11/10/8/7 и Linux.

<img width="1002" height="789" alt="Скриншот интерфейса" src="https://github.com/user-attachments/assets/45a23c6c-b716-4acf-8281-63d35cac8457" />

### Скачать (Portable ZIP)

[![Всего загрузок](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=загрузок&logo=github&style=flat-square)](https://github.com/throneproj/Throne/releases)

### RPM-репозиторий

[Throne RPM-репозиторий](https://parhelia512.github.io/) для Fedora/RHEL и openSUSE/SLE.

---

## Поддерживаемые протоколы

- SOCKS
- HTTP(S)
- Shadowsocks
- Trojan
- VMess
- VLESS
- TUIC
- Hysteria / Hysteria2
- AnyTLS
- WireGuard
- SSH
- Пользовательский outbound
- Пользовательская конфигурация
- Цепочка outbound'ов
- Дополнительное ядро (Extra Core)

## Форматы подписок

Поддерживаются различные форматы: ссылки для передачи конфигураций, JSON-массивы outbound'ов, формат v2rayN, а также ограниченная поддержка Shadowsocks и Clash.

---

## Сборка из исходников

### Требования

| Компонент | Минимальная версия |
|-----------|-------------------|
| CMake     | 3.20              |
| Qt        | 6.5               |
| Go        | 1.21              |
| C++       | C++20             |

### Linux (Ubuntu/Debian)

```bash
# Установка зависимостей
sudo apt-get install -y \
  cmake ninja-build \
  libx11-dev libxcb-cursor-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libgl1-mesa-dev libdbus-1-dev libwayland-dev

# Установка Qt 6 (через aqt или системный пакетный менеджер)
pip install aqtinstall
aqt install-qt linux desktop 6.9.0 gcc_64

# Сборка Go-бэкенда
export GOOS=linux GOARCH=amd64
./scripts/ci/setup_go.sh
./scripts/ci/build_go.sh

# Сборка C++ фронтенда
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja
```

### Windows

```bat
REM Требует MSVC + Go + CMake
REM Сборка статического Qt (опционально, для одного exe-файла):
call script\build_qt_static_windows.bat 6.9.0

REM Сборка Go-бэкенда (в Git Bash / MSYS2):
set GOOS=windows
set GOARCH=amd64
bash scripts/ci/build_go.sh

REM Сборка C++ фронтенда (Developer Command Prompt для MSVC):
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=..\qt6\build\lib\cmake ..
ninja
```

---

## Архитектура

```
Throne (Qt GUI, C++)
        │
        │ gRPC (HTTP/2)
        ▼
NekoCore (Go)
  ├── sing-box    — основной прокси-движок
  └── xray-core   — поддержка VLESS/XTLS
```

- **Фронтенд** (`src/`, `include/`) — Qt 6 GUI на C++20. Управляет конфигурацией, подписками, маршрутизацией, статистикой трафика.
- **Бэкенд** (`core/`) — Go-бинарник, содержащий sing-box и Xray-core. Общается с GUI через gRPC.
- **Конфиг** — генерируется в C++ (`src/configs/generate.cpp`) и передаётся в ядро через gRPC.
- **Подписки** (`src/configs/sub/GroupUpdater.cpp`) — загрузка, разбор и обновление групп профилей.

---

## Часто задаваемые вопросы

**Чем этот проект отличается от оригинального Nekoray?**
Разработчик Nekoray частично забросил проект в декабре 2023 года; в последнее время были сделаны незначительные обновления, но проект официально архивирован. Данный проект был создан, чтобы продолжить путь оригинала — с большим количеством улучшений, новых функций и удалением устаревшего кода.

**Почему антивирус обнаруживает Throne и/или его ядро как вредоносное ПО?**
Встроенная функция обновления Throne загружает новый релиз, удаляет старые файлы и заменяет их новыми — это похоже на действия некоторых вредоносных программ. Функция «System DNS» изменяет системные настройки DNS, что также считается опасным рядом антивирусов.

**Действительно ли нужно устанавливать бит SUID в Linux?**
Для создания системного TUN-интерфейса требуются права root. Без них нужно вручную выдавать ядру необходимые `Cap_xxx_admin`. Можно также отключить автоматическое повышение прав в `Основные настройки → Безопасность`, но функции, требующие root, перестанут работать.

**Почему после принудительного завершения Throne перестаёт работать интернет?**
Если Throne принудительно завершён при активном «System proxy», системный прокси не сбрасывается. Решение: откройте Throne заново, включите «System proxy», затем выключите — это сбросит настройки.

**Откуда берутся загружаемые маршрутные профили/наборы правил?**
Они расположены в репозитории [routeprofiles](https://github.com/throneproj/routeprofiles).

---

## Благодарности

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [simple-protobuf](https://github.com/tonda-kriz/simple-protobuf)
- [quirc](https://github.com/dlbeer/quirc)
- [QHotkey](https://github.com/Skycoder42/QHotkey)
- [clash2singbox](https://github.com/xmdhs/clash2singbox)

---

## Лицензия

GPL-2.0-or-later
