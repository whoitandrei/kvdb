# kvdb — мини key-value БД на C++ со своим thread pool

Учебный пет-проект: маленькая key-value база данных, по духу близкая к Redis.

## Идея и цель

Каждый слой в проекте написан вручную:

- **thread pool** — очередь задач + N воркеров, `mutex` + `condition_variable`;
- **TCP-сервер** — сырые системные вызовы (`socket`/`bind`/`listen`/`accept`/`poll`);
- **хранилище** — простой текстовый протокол и `std::unordered_map` под мутексом.
- **протокол** - простой текстовый протокол общения. Поддерживает команды `SET/GET/DEL`
- 

## Стек

- **Язык:** C++20.
- **Сборка:** CMake ≥ 3.20, генератор Makefiles.
- **Тесты:** GoogleTest, подключается через `FetchContent`.
- **Санитайзеры:** ThreadSanitizer и AddressSanitizer как отдельные типы
  сборки.
- **Платформа разработки:** macOS (Apple Clang). Также собирается на Linux.
- **Бенчмарки** - самописный бенч, похожий на redis-benchmark. Использует HdrHistogram для подсчета перцентилей.

## Структура проекта

```
kvdb/
├── CMakeLists.txt
├── src/ - исходники
├── tests/ - тесты
├── tools/ - доп. тулзы (бенчмарки и тд)
└── build/ - создается при сборке
```

## Сборка

```bash
# обычная отладочная сборка
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug

# сборка с ThreadSanitizer — обязательна перед тем, как считать
# многопоточный код готовым
cmake -B build/tsan -DCMAKE_BUILD_TYPE=TSan
cmake --build build/tsan

# сборка с AddressSanitizer + UBSan — для RAII-классов и работы с
# сырыми дескрипторами/указателями
cmake -B build/asan -DCMAKE_BUILD_TYPE=ASan
cmake --build build/asan
```

Флаг `-DBUILD_BENCHMARK=ON` добавляет сборку нагрузочного бенчмарка
будет лежать в build/.../tools/benchmark

`-fsanitize=thread` и `-fsanitize=address` несовместимы в одной сборке,
отсюда и разделение на типы.

### Тесты

```bash
ctest --test-dir build/debug --output-on-failure
ctest --test-dir build/tsan  --output-on-failure
```

### Запуск

```bash
./build/debug/kvdb
```

## Архитектура

```
main
 └─ TcpServer::run(pool, handler)
     ├─ accept() новых соединений (через poll, вместе с self-pipe)
     └─ на каждое соединение: pool.submit(Task)
                                  └─ handler(Socket) выполняется в воркере
```

## Дальнейшие шаги

- WAL: append-only лог команд и replay при старте.
- Логгер с синхронизацией вывода (сейчас `std::cerr`).
