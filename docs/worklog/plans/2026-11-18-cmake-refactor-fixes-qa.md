# Задание тестеру: повторная проверка после исправлений по обзору OCR

Адресат: tester
Дата: 2026-11-18
Репозиторий: `D:\Projects\Client-Server-exchange`, ветка `cmake-refactor`
Цикл: 2/2

## Цель

Проверить, что закрыты замечания обзора OpenCodeReview (48 находок → пункты A1–A17, B1–B2,
C1–C13 из `docs/worklog/plans/2026-11-18-cmake-refactor-fixes-dev.md`) и что прежние
требования ТЗ/design doc не поехали. Разработчик отчитался `PARTIAL` только из-за того, что
не смог инжектировать ESC в песочнице — этот пункт тоже проверь своим способом.

## Входные данные

1. Задание на исправления: `docs/worklog/plans/2026-11-18-cmake-refactor-fixes-dev.md`
   (там все пункты и требования к проверке).
2. Предыдущее задание проверки: `docs/worklog/plans/2026-11-17-cmake-refactor-qa.md`
   (T1–T12, Q1–Q10 — они остаются в силе).
3. Арх-план и design doc — для требований ТЗ.

## Шаг 0. Чистая сборка

```
cmake -S . -B build-qa -G "Visual Studio 17 2022" -A x64 -DBOOST_ROOT=D:/libs/boost_1_92_0
cmake --build build-qa --config Release
ctest --test-dir build-qa -C Release --output-on-failure
```
Ожидание: 59 тестов (было 43), 100% passed, предупреждений нет. Свой `build-qa` — удали
только СВОЙ прежний `build-qa` каталог при необходимости пересборки (или собери в новый).

## Шаг 1. Регрессия ТЗ (обязательно)

| ID | Проверка |
|---|---|
| R1 | T1–T12 из `2026-11-17-cmake-refactor-qa.md` — все PASS (номера тестов те же; `ctest -R` по suite'ам) |
| R2 | Реальный `numbers.dmp` после прогона сервера: бинарный, размер кратен 4, значения 0..1023 по возрастанию, перезаписывается |
| R3 | Глобальное хранилище: число второго клиента меняет ответ первому (процессный уровень или `IntegrationServer.TwoClientsShareStore`) |
| R4 | Штатная остановка: `stop()` при подключённом клиенте < 2 с, клиент получает EOF; **плюс** попробуй ещё раз реальный ESC (у тебя в прошлый раз получилось) — если среда не даёт, зафиксируй это явно |
| R5 | Логи: консоль + файл, формат `%D %H:%M:%S.%e %05t %05l %v` |
| R6 | Ничего не закоммичено (`git log --oneline -3`), старые MSBuild-файлы отсутствуют (удалены), `spdlog-1.x` и `TestTask.docx` на месте, `docs/` не изменялись |

## Шаг 2. Пункты исправлений — доказательства

По каждому пункту дай доказательство (команда/факт), не «по словам разработчика».

| ID | Пункт | Что проверить |
|---|---|---|
| V1 | A1 | `cse_client.exe` без сервера: процесс завершается **сам** (не ждёт ESC), код 1, время ≈ 6 с, в `client.log` 3 `client connect error` + `client end` |
| V2 | A2 | `cse_server.exe --port 0` → код 2 + usage; `cse_server.exe --dump-interval 0` → код 2 + usage; дамп не создан |
| V3 | A3 | Обзор кода `dump_writer.cpp`: удаление цели только при `std::errc::file_exists`. **Практика:** удерживая `numbers.dmp` открытым с `FileShare.None`, запусти сервер с `--dump-interval 1` — прежний дамп должен остаться на месте (не удалён), в логе ошибка замены |
| V4 | A4 | Обзор кода: `write_once()` в `try/catch`, лог через `log_.error`, поток продолжает жить (плюс тест, если есть) |
| V5 | A5 | `Tests/dump_writer_test.cpp` не трогает `numbers_store` из двух потоков (обзор кода + тест зелёный) |
| V6 | A6 | Тесты `StopSource.ThrowingHandler*` зелёные; в коде `notify_all` гарантирован RAII |
| V7 | A7 | Обзор кода: `attempts_ = 0` при успешном connect; `IntegrationClient.ThreeFailedAttemptsExitWithError` по-прежнему зелёный |
| V8 | A8 | Обзор кода `server.h`: любая ошибка accept, кроме `operation_aborted`, логируется и перевзводит `async_accept` |
| V9 | A9 | Обзор кода `session.h`: тело `on_read` в `try/catch`, исключение не выходит из `io_context::run()` |
| V10 | A10 | В `logger.cpp` есть `flush_on(warn)`. **Практика:** получи строку `error` в логе и убей процесс без штатной остановки (`Stop-Process -Force`) — строка должна быть в файле |
| V11 | A11 | Каталог с именем `server.log` (или недоступный CWD) → процесс печатает причину в stderr и возвращает ненулевой код, без креша |
| V12 | A12 | Занятый порт (эксклюзивный `TcpListener`, как ты делал в прошлый раз — два `cse_server` биндятся из-за отсутствия монопольного бина) → понятное сообщение + ненулевой код |
| V13 | A13, A14 | Тесты клиента/`stop_source` не вешают бинарь при регрессе (обзор RAII-обёртки и `wait_for`) |
| V14 | A15 | `test_support.h`: общий дедлайн на connect/чтение через `steady_timer`; `make_temp_dir` содержит pid (проверь каталог после прогона) |
| V15 | A16 | Интеграционные тесты проверяют `numbers.dmp` (размер N×4, содержимое) и `WritesDumpWhileRunning` зелёный |
| V16 | A17 | `NumbersStore.RejectsOutOfRange` зелёный; `MIN_NUMBER/MAX_NUMBER` в `numbers.h`, `random_source.h` их использует |
| V17 | B1 | Тесты `Session.SubscribersDoNotGrowAfterDisconnects` / `StopSource.UnsubscribeKeepsSubscriberCountFlat` зелёные; `IStopSource` получил `unsubscribe`, `session::close()` им пользуется |
| V18 | B2 | `uniform_int_distribution` — член `MtRandomSource` |
| V19 | C1–C6 | `.gitignore` содержит `build-*/`; `make_shared`; `create(file, level)` + тест `DebugLevelWritesDebugMessages`; прямые include на месте; `size()/mean_of_squares()` `const`; `encode_*(value, out, size) -> bool` и все вызовы обновлены |
| V20 | C7–C13 | Тесты протокола не зависят от endianness (есть `nullptr`/малый буфер), `Server/main.cpp` без лишнего `request_stop()`, граничные тесты `options`, пустой репозиторий → `nullopt`, guard в `start()`, `stopping_ = false`, `yield()` в spin-тесте |

## Чего НЕ делать

- Не править код, тесты, CMake, задания и design doc — только проверка и отчёт.
- Не коммитить, не удалять файлы (кроме своего `build-qa` при необходимости), не чистить
  `1..10`, `x64`, `build`, `_build`.
- Скриншоты не нужны: только текстовые доказательства.

## Ожидаемый результат

Таблица `ID → PASS/FAIL → доказательство` по R1–R6 и V1–V20, отдельно — расхождения и
риски, итоговый статус `PASS` | `FAIL` | `BLOCKED`.
