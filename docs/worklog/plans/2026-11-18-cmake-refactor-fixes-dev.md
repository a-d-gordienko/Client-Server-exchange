# Задание разработчику: исправления по обзору OpenCodeReview (48 замечаний)

Адресат: developer
Дата: 2026-11-18
Ветка: `cmake-refactor`, репозиторий `D:\Projects\Client-Server-exchange`
Цикл исправлений: 1

## Цель

Закрыть замечания обзора OCR (31 файл, 48 находок) и два пункта тестера. Ничего сверх
списка не рефакторить: задачи и архитектура не меняются, меняются дефекты и тесты.

## Входные данные

- Отчёт обзора: находки перечислены ниже с путями и строками (это и есть ТЗ на правку).
- Арх-план: `docs/worklog/plans/2026-11-17-cmake-refactor-arch.md` (структура, тесты T1–T12).
- Design doc: `docs/superpowers/specs/2026-11-17-cmake-refactor-design.md` (поведение по ТЗ).
- Текущий код: `Core/include/cse/*.h`, `Core/src/*.cpp`, `Server/main.cpp`, `Client/main.cpp`, `Tests/*`.

## Блок A — баги (обязательно)

| # | Место | Что сделать |
|---|---|---|
| A1 | `Client/main.cpp:31` | Процесс должен завершаться сам, как только `app.run()` вернул код (сейчас висит в `_getch()`). Опрос — без busy-spin (шаг 20–50 мс), ESC остаётся для ещё работающего клиента; `runner.join()` ровно один раз; код возврата сохраняется (1 после 3 неудачных попыток) |
| A2 | `Core/include/cse/options.h:47,53` | Отклонять `--port 0` и `--dump-interval 0` (возврат `nullopt`): сейчас 0 даёт эфемерный порт и tight-loop дампера |
| A3 | `Core/src/dump_writer.cpp:74-78` | Fallback `remove+rename` — только когда ошибка действительно «цель существует» (`std::errc::file_exists` / `ERROR_ALREADY_EXISTS`). При прочих ошибках (permission, busy) — лог и выход, **не удалять прежний дамп** |
| A4 | `Core/src/dump_writer.cpp:44-56` | `try/catch` вокруг тела цикла и `write_once()`: исключение (`bad_alloc`, ошибка `filesystem`, исключение из `snapshot_fn_`) логируется через `log_.error`, сервер продолжает работу вместо `std::terminate` |
| A5 | `Tests/dump_writer_test.cpp:76` | Убрать гонку в тесте: `numbers_store` без мьютекса нельзя трогать из двух потоков — использовать `locked_repository` (как в проде) либо лок |
| A6 | `Core/include/cse/stop_source.h:55-58` | Broadcast, устойчивый к исключениям: каждый обработчик в своём `try/catch`, `cv_.notify_all()` гарантирован (RAII/scope guard) — иначе `wait()` висит навсегда. Тест: обработчик, бросающий исключение, не мешает остальным и не вешает `wait()` |
| A7 | `Core/include/cse/client.h:62-66` | Сбрасывать `attempts_ = 0` после успешного `connect` (лимит 3 попытки — на цикл переподключения, а не на всю жизнь процесса). T12 не должен сломаться: 3 неудачных connect подряд → код 1 |
| A8 | `Core/include/cse/server.h:52` | При ошибке accept, отличной от `operation_aborted`, логировать и перевзводить `async_accept` (без busy-retry) — сейчас любая транзиентная ошибка навсегда выключает приём |
| A9 | `Core/include/cse/session.h:61-62` | `try/catch` вокруг тела `on_read` (`repo_.add`, `log_.info`): исключение не должно вылетать из `io_context::run()`; лог + закрытие сессии |
| A10 | `Core/src/logger.cpp:33-35` | `engine->flush_on(spdlog::level::warn)` (как минимум): сейчас буфер CRT теряется при аварийном завершении, а ESC — единственный штатный выход |
| A11 | `Core/src/logger.cpp:27`, `Server/main.cpp:22`, `Client/main.cpp:24` | `spdlog_ex` (недоступный/незаписываемый `*.log`) не должен убивать процесс: ловить в `main`, сообщение в `stderr`, ненулевой код возврата |
| A12 | `Server/main.cpp:22-24` | `try/catch` вокруг создания/запуска сервера: занятый порт (`boost::system::system_error`) → понятное сообщение и ненулевой код вместо креша |
| A13 | `Tests/integration_client_test.cpp:75` | При `ASSERT`-таймауте поток `runner` остаётся joinable → `std::terminate`. RAII-обёртка (join в деструкторе) или `join` до любого раннего выхода |
| A14 | `Tests/stop_source_test.cpp:30-34` | Убрать 50 мс `sleep_for` (детерминированный handshake: `promise`/флаг перед `wait()`), `join` с таймаутом — регресс должен давать FAIL, а не зависание всего тест-бинаря |
| A15 | `Tests/support/test_support.h:15,28,116` | (а) общий дедлайн на чтение, а не только `SO_RCVTIMEO` на `recv` (async read + `steady_timer`/`cancel`), таймаут на connect; (б) `make_temp_dir` — уникальный каталог на процесс (`..._<pid>`); (в) порядок include: `winsock2.h` до Boost или убрать лишний |
| A16 | `Tests/integration_server_test.cpp:30` | Добавить интеграционную проверку дампа: после `stop()` файл `numbers.dmp` существует, размер = N×4, содержимое = сохранённые числа (`uint32`, по возрастанию). Плюс тест с малым интервалом: дамп появляется, пока сервер работает |
| A17 | `Core/include/cse/numbers.h:58` | Контракт 0..1023 задокументировать и проверять в `numbers_store::add` (значение вне диапазона → `false`, сумма не растёт); константы `MIN_NUMBER`/`MAX_NUMBER` держать в одном месте (сейчас живут в `random_source.h`) — использовать их и в репозитории, и в генераторе |

## Блок B — производительность (обязательно)

| # | Место | Что сделать |
|---|---|---|
| B1 | `Core/include/cse/stop_source.h`, `Core/include/cse/session.h:29`, `Core/include/cse/server.h:20` | Ввести отписку: `subscribe()` возвращает идентификатор, `IStopSource::unsubscribe(id)`; `session::finish()` отписывается. Сейчас вектор обработчиков растёт с каждым завершённым соединением за весь аптайм. Тест: после 50 циклов connect/disconnect число подписчиков не растёт |
| B2 | `Core/include/cse/random_source.h:28` | `std::uniform_int_distribution` — член класса, а не объект на каждый `next()` |

## Блок C — замечания low (тоже сделать)

- C1 `Core/../.gitignore`: добавить `build-*/` (покрывает `build/` и `build-qa/`).
- C2 `Core/src/logger.cpp:36`: `std::make_shared<SpdlogLogger>(...)` вместо `shared_ptr(new ...)`.
- C3 `Core/include/cse/logger.h:17`: `debug()` обещан интерфейсом, но уровень жёстко `info`. Сделать уровень параметром `SpdlogLogger::create(file, level = spdlog::level::info)` и привести к нему оба sink'а + logger; тест — при `level::debug` запись `debug` попадает в файл.
- C4 Прямые include: `mean_of_squares.h` (+`<cstdint>`, `<mutex>`), `options.h` (+`<system_error>`), `dump_writer.cpp` (+`<string>`); убрать неиспользуемые: `stop_source.h` (`<atomic>`), `Tests/random_source_test.cpp` (`<vector>`).
- C5 `Core/include/cse/numbers.h:35`: `locked_repository::size()`/`mean_of_squares()` — `const` (`mtx_` → `mutable`), как у `numbers_store`.
- C6 `Core/include/cse/protocol.h:20`: `encode_request`/`encode_response` принимают размер буфера и возвращают `bool` (симметрично `decode_*`), обновить вызовы и тесты; гарантировать отказ при `nullptr`/малом размере.
- C7 `Tests/protocol_test.cpp:11,30,36`: убрать привязку к little-endian раскладке (оставить roundtrip; байтовые проверки — только под `if constexpr` по endianness); добавить кейсы `decode_*(nullptr, ...) == false`.
- C8 `Server/main.cpp:30`: убрать лишний `app.stop_source().request_stop()` (если `app.stop()` уже его делает — проверить и оставить один путь).
- C9 `Tests/options_test.cpp:51`: граничные случаи — `--port 0`, `--dump-interval 0`, огромное число, нечисловое, `-1`, `12abc`, пустой `--host`.
- C10 `Tests/locked_repository_test.cpp`: кейс «пустой репозиторий → `mean_of_squares()` == `nullopt`» (ветка реального пути сервера).
- C11 `Core/include/cse/server.h:89`: `start()` не должен запускать второй поток (`io_thread_.joinable()` guard), `stop()` до `start()` — детерминированное поведение, задокументировать.
- C12 `Core/src/dump_writer.cpp:26`: `stopping_ = false` в `start()` (или явно запретить повторный `start()` с проверкой) — сейчас после `stop()` объект неработоспособен без документации.
- C13 `Tests/locked_repository_test.cpp:62`: `std::this_thread::yield()` в spin-цикле.

## Чего НЕ делать

- Не коммитить (шаги commit пропустить).
- Не менять протокол (4/8 байт, host order), порт по умолчанию 64000, формат и имя дампа
  (`numbers.dmp`, `uint32` подряд), имена логов.
- Не трогать `spdlog-1.x/**`, `TestTask.docx`, `docs/**`.
- Не удалять и не перемещать файлы; никаких `git clean`/`git restore`/`git add -A`.
- Не рефакторить архитектуру сверх списка (никаких новых слоёв/интерфейсов, кроме отписки B1
  и параметра уровня логирования C3).

## Проверки (обязательны)

```
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
- Все прежние 43 теста + новые — зелёные, предупреждений не добавилось.
- Ручная проверка после правок:
  - `cse_client.exe` без сервера: 3 попытки → процесс **сам** завершается с кодом 1 (A1);
  - `cse_server.exe --dump-interval 0` → отказ с кодом 2 (A2);
  - сервер + 2 клиента ~15 с: `numbers.dmp` бинарный, `server.log`/`client.log` пишутся, ESC завершает всё без зомби.

## Ожидаемый результат

Отчёт: по каждому пункту A/B/C — сделано/не сделано и как проверено; вывод `ctest`;
список изменённых файлов; что осталось открытым (если что-то не удалось). Последняя строка —
статус `DONE` | `PARTIAL` | `BLOCKED`.

## Критерии готовности

- Все пункты A1–A17, B1–B2, C1–C13 закрыты или явно обоснованы как невыполнимые.
- `ctest` зелёный, включая новые тесты на: отписку подписчиков (B1), бросающий обработчик
  (A6), отказ `--port 0`/`--dump-interval 0` (A2/C9), дамп в интеграционном тесте (A16),
  отказ по диапазону чисел (A17), `debug`-уровень (C3), `encode_*` с `nullptr`/малым буфером (C6).
- Ни одно требование ТЗ не ухудшено: глобальное хранилище, бинарный дамп, ESC-остановка,
  логи в консоль и файл.
