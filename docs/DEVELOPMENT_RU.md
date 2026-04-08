# Руководство по разработке Throne

**Версия:** 0.5.0  
**Дата:** 2026-03-12

---

## 1. Быстрый старт

### 1.1. Клонирование репозитория

```bash
git clone --recursive https://github.com/rakib34343/Neko_Throne.git
cd Neko_Throne
```

### 1.2. Структура директорий

```
Neko_Throne/
├── src/               # C++ исходный код
├── include/           # C++ заголовочные файлы
├── core/server/       # Go-бэкенд
├── 3rdparty/          # Встроенные зависимости
├── cmake/             # CMake-модули
├── res/               # Ресурсы (иконки, переводы)
├── script/            # Deploy-скрипты
├── scripts/ci/        # CI/CD скрипты
└── docs/              # Документация
```

---

## 2. Сборка

### 2.1. Linux

**Требования:**
- GCC 12+ или Clang 16+
- CMake ≥ 3.20
- Qt 6.5+ (Widgets, Network, Concurrent, DBus, LinguistTools)
- Go 1.21+

**Установка Qt через aqt:**
```bash
pip install aqtinstall
aqt install-qt linux desktop 6.9.0 gcc_64
export Qt6_DIR=$HOME/Qt/6.9.0/gcc_64/lib/cmake
```

**Системные зависимости (Ubuntu 22.04+):**
```bash
sudo apt-get install -y \
  cmake ninja-build \
  libx11-dev libx11-xcb-dev libxcb-cursor-dev \
  libxkbcommon-dev libxkbcommon-x11-dev \
  libgl1-mesa-dev libdbus-1-dev libwayland-dev \
  libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
  libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
  libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev
```

**Сборка:**
```bash
# 1. Go-бэкенд
export GOOS=linux GOARCH=amd64
./scripts/ci/setup_go.sh      # установка инструментов protoc
./scripts/ci/build_go.sh      # результат: deployment/linux-amd64/

# 2. C++ фронтенд
mkdir build && cd build
cmake -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  ..
ninja -j$(nproc)
```

### 2.2. Windows

**Требования:**
- Visual Studio 2022 (MSVC v143)
- CMake ≥ 3.20
- Qt 6.5+ (статическая сборка для одного exe)
- Go 1.21+
- Git Bash или MSYS2 для скриптов

**Статическая сборка Qt:**
```bat
call script\build_qt_static_windows.bat 6.9.0
```

**Сборка Go:**
```bash
# В Git Bash
set GOOS=windows GOARCH=amd64
bash scripts/ci/build_go.sh
```

**Сборка C++:**
```bat
REM В Developer Command Prompt для VS 2022
mkdir build && cd build
cmake -GNinja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_PREFIX_PATH=..\qt6\build\lib\cmake ^
  ..
ninja
```

### 2.3. Сборка через Distrobox (Рекомендуется для Bazzite/SteamOS)

Если вы используете атомарный дистрибутив (Bazzite, Fedora Silverblue), рекомендуется использовать контейнер для сборки, чтобы не загрязнять основную систему.

**Создание контейнера (на базе Arch Linux):**
```bash
distrobox create -n dev-qt -i docker.io/library/archlinux:latest
distrobox enter dev-qt
```

**Установка зависимостей внутри контейнера:**
```bash
sudo pacman -S --noconfirm \
  gcc cmake ninja go protobuf protobuf-c ccache \
  qt6-base qt6-declarative qt6-tools qt6-5compat qt6-svg qt6-wayland
```

**Запуск автоматизированной сборки:**
```bash
# Из корня проекта внутри контейнера
./script/build_all.sh
```

### 2.4. Ускорение сборки (ccache)

Компилятор-кэш ccache существенно ускоряет повторные сборки:

```bash
# Linux
sudo apt-get install ccache

# macOS
brew install ccache

# Windows (через chocolatey)
choco install ccache

# Использование в CMake (уже встроено в CI):
cmake -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ..
```

---

## 3. Разработка

### 3.1. Добавление нового протокола

1. **Создать структуру данных:**
   ```
   include/configs/outbounds/myprotocol.h     # данные протокола
   src/configs/outbounds/myprotocol.cpp        # реализация
   ```
   Пример: `include/configs/outbounds/vmess.h`

2. **Добавить парсинг ссылок** в `src/configs/proxy/Link2Bean.cpp`:
   ```cpp
   if (link.startsWith("myproto://")) {
       ent = Configs::ProfileManager::NewProxyEntity("myprotocol");
       auto ok = ent->MyProtocol()->TryParseLink(link);
       if (!ok) return;
   }
   ```

3. **Добавить сериализацию в ссылку** в `src/configs/proxy/Bean2Link.cpp`.

4. **Добавить генерацию конфига** в `src/configs/proxy/Bean2CoreObj_box.cpp`.

5. **Создать UI-редактор:**
   ```
   include/ui/profile/edit_myprotocol.h
   include/ui/profile/edit_myprotocol.ui   # Qt Designer
   src/ui/profile/edit_myprotocol.cpp
   ```

6. **Зарегистрировать** в `include/ui/profile/dialog_edit_profile.h` и `dialog_edit_profile.cpp`.

7. **Добавить в CMakeLists.txt** новые файлы в `PROJECT_SOURCES`.

8. **Обновить переводы:**
   ```bash
   ./scripts/translate.sh
   ```

### 3.2. Добавление новой локали

1. Создать файл перевода:
   ```bash
   cp res/translations/ru_RU.ts res/translations/de_DE.ts
   ```

2. Перевести строки в `de_DE.ts` (XML-формат Qt Linguist).

3. Добавить файл в `CMakeLists.txt`:
   ```cmake
   set(TS_FILES
       res/translations/zh_CN.ts
       res/translations/fa_IR.ts
       res/translations/ru_RU.ts
       res/translations/de_DE.ts   # ← добавить
   )
   ```

4. Зарегистрировать в `TranslationManager`.

### 3.3. Форматирование кода

```bash
# Применить clang-format ко всем файлам src/ и include/
./scripts/format_cpp.sh

# Проверка без изменений (CI-режим)
./scripts/ci/lint.sh
```

Конфиг форматирования: `.clang-format` (если отсутствует — используется стиль по умолчанию из lint.sh).

---

## 4. Отладка

### 4.1. Отладка C++ части

**Linux (GDB):**
```bash
# Сборка с символами отладки
cmake -DCMAKE_BUILD_TYPE=Debug ..
ninja

# Запуск с GDB
gdb ./Neko_Throne
(gdb) run
```

**Windows (MSVC debugger):**
- Открыть `build/Neko_Throne.sln` в Visual Studio
- Поставить точки останова
- F5 → запуск с отладчиком

### 4.2. Отладка Go-бэкенда

```bash
cd core/server

# Запуск с включённым race detector
go run -race . serve

# Тесты
go test ./...

# Профилирование
go run . serve --pprof-port 6060
# затем: go tool pprof http://localhost:6060/debug/pprof/heap
```

### 4.3. gRPC-трассировка

Включить verbose-логирование gRPC:
```bash
GRPC_GO_LOG_VERBOSITY_LEVEL=99 GRPC_GO_LOG_SEVERITY_LEVEL=info ./NekoCore serve
```

### 4.4. Воспроизведение зависания при переключении профиля (Windows + VLESS)

Для воспроизведения известной проблемы зависания:
1. Создать два профиля с протоколом VLESS
2. Запустить первый профиль
3. Немедленно переключиться на второй
4. Наблюдать состояние через логи (`Core exited`, `Restarting the core`)

**Ожидаемое поведение после исправления:**
- В логах появляется `Core state changed to not running` (через CoreLifecycleState)
- Новый процесс стартует без зависания
- UI обновляется корректно

### 4.5. Просмотр логов ядра

Логи ядра выводятся в UI в реальном времени (вкладка «Log»). Для сохранения в файл:
```bash
# Linux: перенаправление через pipe
./Neko_Throne 2>&1 | tee throne.log
```

---

## 5. Тестирование

### 5.1. Запуск unit-тестов Go

```bash
cd core/server
go test ./... -v
```

### 5.2. CI-тесты

Все тесты автоматически запускаются при push в GitHub Actions:
- Линтинг C++ (`cppcheck` + `clang-format`)
- Сборка на Linux и Windows
- Публикация релиза (только при push в `main` или `workflow_dispatch`)

### 5.3. Ручная проверка протоколов

Для проверки импорта/экспорта профилей:
1. Создать профиль нужного протокола
2. Экспортировать ссылку (ПКМ → Share)
3. Импортировать ссылку в новую группу
4. Сравнить параметры исходного и импортированного профиля

### 5.4. Проверка anti-leak (NetworkLeakGuard)

```bash
# Убедиться, что DNS-запросы не утекают:
# 1. Включить VPN-режим (TUN)
# 2. Запустить DNS-leak тест: https://dnsleaktest.com
# 3. Результат должен показывать только адрес DNS-сервера из конфига
```

---

## 6. CI/CD

### 6.1. Структура воркфлоу

```
.github/workflows/main_ci.yml
  ├── lint          — статический анализ, форматирование
  ├── build         — сборка (матрица: linux × windows)
  │     ├── Go backend
  │     └── C++ frontend
  └── release       — публикация (только main или workflow_dispatch)
```

### 6.2. Ручной запуск

В GitHub → Actions → «Neko Throne CI/CD (amd64)» → «Run workflow»:
- `tag`: версия релиза (например, `v0.5.1`)
- `publish`: `r` — релиз, `p` — предрелиз, пусто — без публикации

### 6.3. Кэши

| Кэш | Ключ | Путь |
|-----|------|------|
| Go артефакты | `go-<os>-<hash>-<go_ver>` | `go-artifacts.tgz` |
| ccache Linux | `ccache-linux-<hash>` | `~/.ccache` |
| ccache Windows | `ccache-windows-<hash>` | `~/AppData/Local/ccache` |
| Qt Linux | `qt-linux-<ver>` | (auto) |
| Qt Windows static | `win-qt-static-<ver>-<hash>` | `qt6/build` |

---

## 7. Структура кодовых соглашений

- **Язык:** C++20, Qt 6
- **Именование:** `CamelCase` для классов, `snake_case` для переменных и функций, `m_` prefix для приватных полей
- **Пространства имён:** `Configs_sys`, `Configs_network`, `Configs`, `Subscription`
- **Сигналы/слоты:** только type-safe синтаксис (`&ClassName::signal`)
- **Потокобезопасность:** мьютексы только там, где нужно; UI-операции только в главном потоке через `runOnUiThread()`
- **Smart pointers:** `std::unique_ptr` для владения, `std::shared_ptr` для shared ownership
- **Отсутствие naked new:** все объекты с Qt-родителем создаются с `parent` или управляются smart pointer'ами
