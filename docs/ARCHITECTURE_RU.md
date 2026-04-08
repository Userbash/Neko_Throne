# Архитектура Throne

**Версия:** 0.5.0  
**Дата:** 2026-03-12

---

## 1. Общая схема

```
┌─────────────────────────────────────────────────────────────┐
│                    Throne (Qt GUI, C++)                      │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ MainWindow│  │ Settings │  │ Profiles │  │ Routes   │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │          │
│       └─────────────┴─────────────┴─────────────┘          │
│                           │                                 │
│  ┌─────────────────────────▼──────────────────────────────┐ │
│  │                   CoreManager / ProxyStateManager       │ │
│  │              DataStore (SQLite, runtime config)         │ │
│  └────────────────────────┬───────────────────────────────┘ │
│                           │ gRPC (HTTP/2, port auto)        │
└───────────────────────────┼─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                  NekoCore (Go binary)                        │
│                                                             │
│  ┌────────────────────┐  ┌───────────────────────────────┐  │
│  │    sing-box         │  │         xray-core             │  │
│  │  (основной движок)  │  │  (VLESS/XTLS/XHTTP/Reality)  │  │
│  └────────────────────┘  └───────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Компоненты фронтенда (C++)

### 2.1. Слой UI (`src/ui/`, `include/ui/`)

| Файл | Описание |
|------|----------|
| `mainwindow.cpp` | Главное окно: список профилей, групп, трафик, логи |
| `mainwindow_rpc.cpp` | Обработка RPC-событий от ядра в UI |
| `ui/core/AsyncBackendBridge.cpp` | Асинхронный мост между UI и бэкендом |
| `ui/core/TranslationManager.cpp` | Управление локализацией (.qm-файлы) |
| `ui/setting/ThemeManager.cpp` | Управление темой оформления |
| `ui/profile/dialog_edit_profile.cpp` | Универсальный диалог редактирования профиля |
| `ui/profile/edit_*.cpp` | Редакторы для каждого протокола (20+ файлов) |
| `ui/group/dialog_manage_groups.cpp` | Управление группами подписок |
| `ui/setting/dialog_basic_settings.cpp` | Основные настройки |
| `ui/setting/dialog_vpn_settings.cpp` | Настройки VPN/TUN |

### 2.2. Слой системы (`src/sys/`, `include/sys/`)

| Файл | Описание |
|------|----------|
| `Process.cpp` / `.hpp` | Жизненный цикл внешнего процесса ядра |
| `CoreManager.cpp` / `.hpp` | Обнаружение, версионирование, обновление ядра |
| `ProxyStateManager.cpp` / `.hpp` | Переключение режимов прокси (Direct / Proxy / Block) |
| `NetworkLeakGuard.cpp` / `.hpp` | Защита от DNS-утечек при смене режима |
| `AutoRun.cpp` | Автозапуск системы |
| `linux/LinuxCap.cpp` | Linux capabilities (TUN, сетевые операции) |
| `windows/WinVersion.cpp` | Определение версии Windows |

### 2.3. Слой конфигурации (`src/configs/`, `include/configs/`)

| Файл | Описание |
|------|----------|
| `generate.cpp` / `.h` | Генерация JSON-конфигурации для sing-box / xray |
| `proxy/Bean2CoreObj_box.cpp` | Конвертация внутреннего профиля → sing-box outbound |
| `proxy/Bean2Link.cpp` | Сериализация профиля → share link (URI) |
| `proxy/Link2Bean.cpp` | Разбор share link → внутренний профиль |
| `proxy/Json2Bean.cpp` | Разбор JSON → внутренний профиль |
| `sub/GroupUpdater.cpp` | Загрузка и обновление группы профилей из подписки |
| `common/TLS.cpp` | Поля TLS (SNI, ALPN, Reality, uTLS) |
| `common/transport.cpp` | Транспортные настройки (WebSocket, gRPC, HTTP/2) |
| `common/multiplex.cpp` | Мультиплексирование (smux, yamux, h2mux) |
| `outbounds/*.cpp` | Структуры данных и сериализация для каждого протокола |

### 2.4. Слой данных (`src/dataStore/`)

| Файл | Описание |
|------|----------|
| `Database.cpp` | SQLite: хранение профилей и групп |
| `ProxyEntity.cpp` | Единица профиля (владеет outbound-объектом) |
| `Group.cpp` | Группа профилей (подписка или пользовательская) |
| `RouteEntity.cpp` | Правила маршрутизации |
| `ProfileFilter.cpp` | Фильтрация профилей по критериям |

### 2.5. Сетевой слой (`src/global/`)

| Файл | Описание |
|------|----------|
| `HTTPRequestHelper.cpp` | HTTP-запросы: подписки, обновления, загрузка ядра |
| `Configs.cpp` | Глобальная конфигурация, DataStore, пути к файлам |
| `Utils.cpp` | Утилиты: строки, потоки, декодирование base64 |
| `CountryHelper.cpp` | Определение страны по IP |
| `DeviceDetailsHelper.cpp` | Информация об устройстве для заголовков подписки |

### 2.6. gRPC API (`src/api/`)

| Файл | Описание |
|------|----------|
| `RPC.cpp` | gRPC HTTP/2-клиент для связи с NekoCore |
| `CoreVersionParser.cpp` | Разбор версии ядра и его доступности |

---

## 3. Компоненты бэкенда (Go)

### 3.1. Структура `core/server/`

| Файл | Описание |
|------|----------|
| `main.go` | Точка входа, инициализация CLI (cobra) |
| `server.go` | gRPC-сервер, управление sing-box и xray-core |
| `server_linux.go` | Linux-специфичная логика |
| `server_windows.go` | Windows-специфичная логика |
| `server_darwin.go` | macOS-специфичная логика |
| `internal/utils.go` | Вспомогательные функции |
| `gen/libcore.proto` | Protobuf-определение API |

### 3.2. Встроенные движки

- **sing-box** — основной прокси-движок (SOCKS, HTTP, VMess, VLESS, Shadowsocks, Trojan, TUIC, Hysteria, WireGuard, TUN, маршрутизация)
- **xray-core** — расширение для VLESS с XTLS, XHTTP, Reality; также поддерживает VMess

---

## 4. Взаимодействие компонентов

### 4.1. Поток запуска профиля

```
MainWindow::profile_start(id)
    │
    ▼
DataStore::LoadProfile(id)           ← читает из SQLite
    │
    ▼
Generate::generateConfig()           ← создаёт JSON конфиг
    │
    ▼
CoreProcess::Start()                 ← запускает NekoCore
    │
    ▼
[NekoCore stdout: "Core listening at port XXXX"]
    │
    ▼
CoreProcess::stateChanged → core_running = true
    │
    ▼
RPC::StartProxy(config)              ← gRPC вызов в NekoCore
    │
    ▼
ProxyStateManager::setMode(Proxy)    ← настраивает системный прокси
```

### 4.2. Поток обновления подписки

```
MainWindow → GroupUpdater::update(url)
    │
    ▼
NetworkRequestHelper::HttpGet(url)   ← загружает контент
    │
    ▼
GroupUpdater::RawUpdater::update()   ← разбор: base64 / JSON / links
    │
    ▼
Link2Bean::TryParseLink()            ← парсинг URI → ProxyEntity
    │
    ▼
DataStore::SaveProfile()             ← сохранение в SQLite
    │
    ▼
MainWindow::refreshProxyList()       ← обновление UI
```

### 4.3. Конечный автомат ядра (CoreLifecycleState)

```
Stopped ──[Start()]──► Starting ──[«Core listening at»]──► Running
   ▲                       │                                  │
   │                [«failed to serve»]               [Kill()/Restart()]
   │                       │                                  │
   │                    Failed                           Stopping
   │                                                         │
   └─────────────────────────[exit]──────────────────────────┘
                                                    Restarting ──► Starting
```

---

## 5. Хранилище данных

### 5.1. SQLite

Все профили, группы и маршрутные правила хранятся в SQLite по пути:
- Linux: `~/.config/Throne/config.db`
- Windows: `%APPDATA%\Throne\config.db`

### 5.2. Конфиги ядра

Генерируются при каждом запуске профиля и сохраняются в:
- Linux: `~/.config/Throne/config.json`
- Windows: `%APPDATA%\Throne\config.json`

### 5.3. Переводы

Файлы `.qm` (скомпилированные переводы Qt) хранятся рядом с исполняемым файлом в директории `lang/`. Загружаются при старте через `TranslationManager`.

---

## 6. Платформенные особенности

| Функция | Linux | Windows |
|---------|-------|---------|
| TUN-интерфейс | Через CAP_NET_ADMIN или SUID | Через WinTUN |
| Системный прокси | GNOME/KDE через D-Bus | WinInet |
| DNS-менеджер | /etc/resolv.conf или systemd-resolved | DNS-настройки адаптера |
| Автозапуск | systemd user unit / .desktop | Реестр HKCU Run |
| Мини-дамп | `core` file | MiniDump API |

---

## 7. Зависимости

### C++ (CMake)

| Зависимость | Версия | Назначение |
|-------------|--------|------------|
| Qt 6 | ≥6.5 | Widgets, Network, Concurrent, DBus, LinguistTools |
| QHotkey | bundled | Глобальные горячие клавиши |
| simple-protobuf (myproto) | bundled | Сериализация для gRPC |
| quirc | bundled | Декодирование QR-кодов |
| base64 | bundled | Base64-кодек |

### Go (go.mod)

| Зависимость | Версия | Назначение |
|-------------|--------|------------|
| sing-box | v1.13.2 | Прокси-движок |
| sing-tun | v0.8.2 | TUN-интерфейс |
| xray-core | v1.260206.0 | Xray движок |
| grpc | v1.79.1 | gRPC сервер |
| protobuf | v1.36.11 | Сериализация |
| cobra | latest | CLI |
