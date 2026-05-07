import pandas as pd
import matplotlib.pyplot as plt

# Читаем новые файлы
output = pd.read_csv("output3D.csv")
particles = pd.read_csv("particles3D.csv")

# Удаляем случайно повторившиеся строки-заголовки, если они есть
particles = particles[particles["step"].astype(str) != "step"].copy()

# Если в output тоже случайно повторились заголовки
if "step" in output.columns:
    output = output[output["step"].astype(str) != "step"].copy()

# Преобразуем данные частиц в числа
particles["step"] = pd.to_numeric(particles["step"])
particles["x"] = pd.to_numeric(particles["x"])
particles["y"] = pd.to_numeric(particles["y"])
particles["z"] = pd.to_numeric(particles["z"])
particles["weight"] = pd.to_numeric(particles["weight"])

# Преобразуем данные основного результата в числа
for column in output.columns:
    output[column] = pd.to_numeric(output[column])

# Если в output нет отдельного столбца step, создаём его
if "step" not in output.columns:
    output["step"] = range(len(output))

# Берём частицы только с последнего шага,
# иначе на графике будет слишком много точек
last_step = particles["step"].max()
last_particles = particles[particles["step"] == last_step].copy()

# Масштабируем веса для цвета и размера точек
max_weight = last_particles["weight"].max()

if max_weight > 0:
    last_particles["scaled_weight"] = last_particles["weight"] / max_weight
else:
    last_particles["scaled_weight"] = 1.0

# Размер точек: чем больше вес, тем крупнее частица
point_sizes = 10 + 80 * last_particles["scaled_weight"]

fig = plt.figure(figsize=(12, 9))
ax = fig.add_subplot(111, projection="3d")

# Частицы на последнем шаге
scatter = ax.scatter(
    last_particles["x"],
    last_particles["y"],
    last_particles["z"],
    c=last_particles["scaled_weight"],
    s=point_sizes,
    alpha=0.6,
    label="Particles"
)

# Истинная траектория объекта
ax.plot(
    output["true_x"],
    output["true_y"],
    output["true_z"],
    linewidth=3,
    label="True trajectory"
)

# Измерения сенсора
ax.plot(
    output["meas_x"],
    output["meas_y"],
    output["meas_z"],
    linestyle="dotted",
    linewidth=2,
    label="Measurements"
)

# Оценка фильтра
ax.plot(
    output["est_x"],
    output["est_y"],
    output["est_z"],
    linewidth=3,
    label="Estimated trajectory"
)

# Начальная точка
ax.scatter(
    output["true_x"].iloc[0],
    output["true_y"].iloc[0],
    output["true_z"].iloc[0],
    s=100,
    marker="o",
    label="Start"
)

# Конечная точка
ax.scatter(
    output["true_x"].iloc[-1],
    output["true_y"].iloc[-1],
    output["true_z"].iloc[-1],
    s=100,
    marker="X",
    label="End"
)

ax.set_title("3D Particle Filter Visualization")
ax.set_xlabel("X position")
ax.set_ylabel("Y position")
ax.set_zlabel("Z position")

fig.colorbar(scatter, ax=ax, label="Normalized particle weight")

ax.legend()
plt.tight_layout()

plt.savefig("particle_filter_3d.png", dpi=300, bbox_inches="tight")
plt.show()