# AnySpace Engine (ans)

## English

**AnySpace Engine** (short: **ans**) is a game engine for various non-Euclidean spaces — portals, wormholes, hyperspace, curved spaces, and more.

### Tech stack
- **Language:** C
- **Graphics API:** Vulkan
- **Windowing / input:** GLFW
- **Shaders:** GLSL
- **Build system:** Make

New dependencies may be added later, but only if they are truly necessary.

### Status
Currently only the foundation is ready — a triangle. Everything else is a work in progress.

### Building
```bash
make
cd build && ./anyspace
# or in one command:
make test
```

### I write the code myself
AI is used rationally, with my full control over every change.

### Roadmap (rough)
- [x] Window + Vulkan initialization
- [x] First triangle
- [ ] Camera and input handling
- [ ] Portal rendering (non-Euclidean geometry)
- [ ] Wormholes, hyperspace, curved spaces
- [ ] …and more

---

## Русский

**AnySpace Engine** (сокращённо: **ans**) — игровой движок для различных неевклидовых пространств: порталов, червоточин, гиперпространства, искривлений — и другого.

### Технологии
- **Язык:** C
- **Графический API:** Vulkan
- **Окна / ввод:** GLFW
- **Шейдеры:** GLSL
- **Система сборки:** Make

Новые зависимости могут появиться позже, но только если они действительно нужны.

### Статус
Пока готова только основа — треугольник. Всё остальное в процессе.

### Сборка
```bash
make
cd build && ./anyspace
# Или одной командой:
make test
```

### Код я пишу сам
ИИ используется рационально и под моим полным контролем.

### План (примерный)
- [x] Инициализация окна и Vulkan
- [x] Первый треугольник
- [ ] Камера и обработка ввода
- [ ] Рендеринг порталов (неевклидова геометрия)
- [ ] Червоточины, гиперпространство, искривлённые пространства
- [ ] …и многое другое
