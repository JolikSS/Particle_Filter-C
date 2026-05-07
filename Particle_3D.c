#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 500         // Количество частиц (гипотез о положении объекта).
                      // Чем больше частиц - тем точнее оценка,
                      // но тем больше вычислений.

#define STEPS 50      // Количество шагов симуляции (итераций фильтра).
                      // На каждом шаге объект двигается,
                      // получаем измерение и обновляем фильтр.

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Одна частица представляет гипотезу о положении объекта.
// Particle Filter хранит много таких гипотез одновременно.
// В 3D каждая гипотеза содержит не одну координату x,
// а сразу три координаты: x, y и z.
typedef struct
{
    double x;       // Предполагаемая позиция объекта по оси X.
                    // То есть где, по мнению этой частицы,
                    // находится объект по горизонтальной оси.

    double y;       // Предполагаемая позиция объекта по оси Y.
                    // Это вторая координата в трехмерном пространстве.

    double z;       // Предполагаемая позиция объекта по оси Z.
                    // Это третья координата в трехмерном пространстве.

    double vx;      // Скорость частицы по оси X.
    double vy;      // Скорость частицы по оси Y.
    double vz;      // Скорость частицы по оси Z.

    double weight;  // Вес частицы = вероятность того,
                    // что именно эта гипотеза ближе всего к реальности.
                    // Чем ближе частица к измерению в 3D,
                    // тем больше ее вес.
} Particle;


// Статистика распределения частиц.
// Используется для анализа работы фильтра.
// В 3D статистика считается отдельно для каждой координаты.
typedef struct
{
    double mean_x;       // Среднее значение позиции частиц по X.
                         // Это приблизительная оценка положения объекта по X.

    double mean_y;       // Среднее значение позиции частиц по Y.
                         // Это приблизительная оценка положения объекта по Y.

    double mean_z;       // Среднее значение позиции частиц по Z.
                         // Это приблизительная оценка положения объекта по Z.

    double variance_x;   // Дисперсия по X - показывает разброс частиц
                         // вокруг среднего значения по X.

    double variance_y;   // Дисперсия по Y - показывает разброс частиц
                         // вокруг среднего значения по Y.

    double variance_z;   // Дисперсия по Z - показывает разброс частиц
                         // вокруг среднего значения по Z.
                         // Маленькая дисперсия - фильтр уверен.
                         // Большая дисперсия - фильтр не уверен.

    double min_x;        // Минимальное значение позиции частицы по X.
    double max_x;        // Максимальное значение позиции частицы по X.

    double min_y;        // Минимальное значение позиции частицы по Y.
    double max_y;        // Максимальное значение позиции частицы по Y.

    double min_z;        // Минимальное значение позиции частицы по Z.
    double max_z;        // Максимальное значение позиции частицы по Z.
} FilterStats;

// Генерация случайного числа в диапазоне (0,1)
double rand_uniform()
{
    return ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
}

// Генерация случайного числа с нормальным распределением.
// mean   - среднее значение
// stddev - стандартное отклонение
// Используется преобразование Бокса-Мюллера, которое превращает два равномерных числа в одно нормально распределенное.
// Это нужно для моделирования шума:
//  - шума движения
//  - шума измерений
double rand_normal(double mean, double stddev)
{
    double u1 = rand_uniform();
    double u2 = rand_uniform();

    // формула Бокса-Мюллера
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

    // масштабируем под нужное среднее и стандартное отклонение
    return z0 * stddev + mean;
}

/* ---------------- РАБОТА С ПАМЯТЬЮ ---------------- */

// Выделение памяти под массив частиц
Particle* create_particles(int n)
{
    Particle* p = malloc(sizeof(Particle) * n);

    if (!p)
    {
        printf("Memory allocation error\n"); // ошибка выделения памяти
        exit(1);
    }

    return p;
}

// Освобождение памяти
void free_particles(Particle* p)
{
    free(p);
}

/* ---------------- МОДЕЛЬ ДВИЖЕНИЯ И ИЗМЕРЕНИЯ ---------------- */

// Модель реального движения объекта.
// Это "настоящий мир", который фильтр пытается угадать.
// В 3D объект имеет три координаты: x, y, z.
void simulate_true_position(double* x, double* y, double* z)
{
    double vx = 1.0;               // постоянная скорость объекта по X
    double vy = 0.5;               // постоянная скорость объекта по Y
    double vz = 0.2;               // постоянная скорость объекта по Z

    double noise_x = rand_normal(0, 0.5); // случайный шум движения по X
    double noise_y = rand_normal(0, 0.5); // случайный шум движения по Y
    double noise_z = rand_normal(0, 0.5); // случайный шум движения по Z

    // реальная позиция = старая позиция + скорость + случайное отклонение
    *x += vx + noise_x;
    *y += vy + noise_y;
    *z += vz + noise_z;
}

// Модель сенсора.
// Сенсор не видит точную позицию, а возвращает значение с шумом.
// В 3D сенсор возвращает измерение сразу по трем координатам.
void simulate_measurement(
    double true_x,
    double true_y,
    double true_z,
    double* meas_x,
    double* meas_y,
    double* meas_z
)
{
    *meas_x = true_x + rand_normal(0, 2.0);
    *meas_y = true_y + rand_normal(0, 2.0);
    *meas_z = true_z + rand_normal(0, 2.0);
}

/* ==================== ИНИЦИАЛИЗАЦИЯ ФИЛЬТРА ==================== */
// Начальное распределение частиц.
// Мы ничего не знаем о позиции объекта, поэтому распределяем частицы равномерно.
// В 3D каждая частица получает случайные x, y и z.
void init_particles(Particle* particles, int n)
{
    for (int i = 0; i < n; i++)
    {
        // случайная позиция от -10 до 10 по каждой оси
        particles[i].x = -10 + 20 * rand_uniform();
        particles[i].y = -10 + 20 * rand_uniform();
        particles[i].z = -10 + 20 * rand_uniform();

        // начальная скорость по каждой оси
        particles[i].vx = 1.0;
        particles[i].vy = 0.5;
        particles[i].vz = 0.2;

        // все частицы одинаково вероятны
        particles[i].weight = 1.0 / n;
    }
}


/* ---------------- ПРЕДСКАЗАНИЕ ---------------- */
// Мы предполагаем, что частицы движутся так же, как и реальный объект (модель движения).
// К каждой частице добавляется скорость + случайный шум.
// Но скорость теперь не просто постоянная: она берется из экспоненциального среднего.
void predict(
    Particle* particles,
    int n,
    double Q,
    double smoothed_vx,
    double smoothed_vy,
    double smoothed_vz
)
{
    for (int i = 0; i < n; i++)
    {
        // Все частицы используют одну сглаженную скорость фильтра по каждой оси.
        particles[i].vx = smoothed_vx;
        particles[i].vy = smoothed_vy;
        particles[i].vz = smoothed_vz;

        // Предсказание позиции:
        // старая позиция + сглаженная скорость + шум процесса.
        particles[i].x += smoothed_vx + rand_normal(0, sqrt(Q));
        particles[i].y += smoothed_vy + rand_normal(0, sqrt(Q));
        particles[i].z += smoothed_vz + rand_normal(0, sqrt(Q));
    }
}

/* ---------------- ОБНОВЛЕНИЕ ВЕСОВ ---------------- */
// Пересчитываем веса частиц на основе измерения.
// Если частица близко к измерению, ее вес становится большим.
// Если далеко - маленьким.
// В 3D близость считается через расстояние между двумя точками:
// измерением (meas_x, meas_y, meas_z) и частицей (x, y, z).
void update_weights(
    Particle* particles,
    int n,
    double meas_x,
    double meas_y,
    double meas_z,
    double R
)
{
    for (int i = 0; i < n; i++)
    {
        double dx = meas_x - particles[i].x;
        double dy = meas_y - particles[i].y;
        double dz = meas_z - particles[i].z;

        // Квадрат расстояния в трехмерном пространстве.
        double distance_squared = dx * dx + dy * dy + dz * dz;

        // формула гауссовского правдоподобия
        double exponent = -distance_squared / (2.0 * R);

        // защита от численного переполнения
        if (exponent < -50)
            exponent = -50;

        double w = exp(exponent);

        if (!isfinite(w))
            w = 0;

        particles[i].weight = w;
    }
}

/* ---------------- НОРМАЛИЗАЦИЯ ---------------- */

// После обновления веса могут быть любыми.
// Нужно нормализовать их, чтобы сумма всех весов была равна 1.
void normalize_weights(Particle* particles, int n)
{
    double sum = 0;

    for (int i = 0; i < n; i++)
        sum += particles[i].weight;

    // если веса "выродились" (слишком маленькие)
    // возвращаем равномерное распределение
    if (sum < 1e-300)
    {
        for (int i = 0; i < n; i++)
            particles[i].weight = 1.0 / n;

        return;
    }

    // делим каждый вес на сумму
    for (int i = 0; i < n; i++)
        particles[i].weight /= sum;
}

/* ==================== ЭФФЕКТИВНЫЙ РАЗМЕР ВЫБОРКИ (ESS) ==================== */
// ESS показывает, сколько частиц реально "работают".
// Если несколько частиц имеют почти весь вес, остальные бесполезны.
// Тогда нужно делать ресэмплинг.
double compute_ess(Particle* particles, int n)
{
    double sum = 0;

    for (int i = 0; i < n; i++)
        sum += particles[i].weight * particles[i].weight;

    if (sum < 1e-300)
        return 0;

    return 1.0 / sum;
}

/* ---------------- РЕСЭМПЛИНГ ---------------- */

// Частицы с маленьким весом удаляются, а хорошие частицы копируются.
// Это предотвращает вырождение фильтра.
void resample(Particle* particles, int n)
{
    Particle* temp = malloc(sizeof(Particle) * n); // выделяем память
    double* cumulative = malloc(sizeof(double) * n);

    if (!temp || !cumulative)
    {
        printf("Memory allocation error\n");
        free(temp);
        free(cumulative);
        exit(1);
    }

    // Построение cumulative distribution function (CDF)
    // Накопленная сумма весов
    cumulative[0] = particles[0].weight;

    for (int i = 1; i < n; i++)
        cumulative[i] = cumulative[i - 1] + particles[i].weight;

    // Систематическая выборка
    double step = 1.0 / n;
    double r = rand_uniform() * step; // Случайное начальное смещение

    int index = 0;

    for (int i = 0; i < n; i++)
    {
        double u = r + i * step; // Точка выборки

        // Поиск частицы, чей интервал содержит u
        while (index < n - 1 && u > cumulative[index])
            index++;

        // Копируем частицу и сбрасываем вес
        temp[i] = particles[index];
        temp[i].weight = 1.0 / n;
    }

    // Копируем обратно
    for (int i = 0; i < n; i++)
        particles[i] = temp[i];

    free(temp);
    free(cumulative);
}

/* ---------------- ОЦЕНКА СОСТОЯНИЯ ---------------- */

// Финальная оценка положения.
// Это взвешенное среднее всех частиц.
// В 3D нужно отдельно посчитать оценку по X, Y и Z.
void estimate_position(
    Particle* particles,
    int n,
    double* est_x,
    double* est_y,
    double* est_z
)
{
    *est_x = 0;
    *est_y = 0;
    *est_z = 0;

    for (int i = 0; i < n; i++)
    {
        *est_x += particles[i].x * particles[i].weight;
        *est_y += particles[i].y * particles[i].weight;
        *est_z += particles[i].z * particles[i].weight;
    }
}

/* ---------------- СТАТИСТИКА ---------------- */

// Вычисление среднего, дисперсии, минимума и максимума по частицам.
// В 3D статистика считается отдельно по каждой координате.
FilterStats compute_statistics(Particle* particles, int n)
{
    FilterStats stats;

    stats.mean_x = 0;
    stats.mean_y = 0;
    stats.mean_z = 0;

    stats.variance_x = 0;
    stats.variance_y = 0;
    stats.variance_z = 0;

    stats.min_x = particles[0].x;
    stats.max_x = particles[0].x;

    stats.min_y = particles[0].y;
    stats.max_y = particles[0].y;

    stats.min_z = particles[0].z;
    stats.max_z = particles[0].z;

    // Среднее и границы
    for (int i = 0; i < n; i++)
    {
        stats.mean_x += particles[i].x * particles[i].weight;
        stats.mean_y += particles[i].y * particles[i].weight;
        stats.mean_z += particles[i].z * particles[i].weight;

        if (particles[i].x < stats.min_x)
            stats.min_x = particles[i].x;

        if (particles[i].x > stats.max_x)
            stats.max_x = particles[i].x;

        if (particles[i].y < stats.min_y)
            stats.min_y = particles[i].y;

        if (particles[i].y > stats.max_y)
            stats.max_y = particles[i].y;

        if (particles[i].z < stats.min_z)
            stats.min_z = particles[i].z;

        if (particles[i].z > stats.max_z)
            stats.max_z = particles[i].z;
    }

    // Дисперсия
    for (int i = 0; i < n; i++)
    {
        double dx = particles[i].x - stats.mean_x;
        double dy = particles[i].y - stats.mean_y;
        double dz = particles[i].z - stats.mean_z;

        stats.variance_x += particles[i].weight * dx * dx;
        stats.variance_y += particles[i].weight * dy * dy;
        stats.variance_z += particles[i].weight * dz * dz;
    }

    return stats;
}

/* ---------------- ДОВЕРИТЕЛЬНЫЙ ИНТЕРВАЛ ---------------- */

// Приближенный доверительный интервал 95%: mean +/- 1.96 * std.
// Для нормального распределения 95% значений лежит примерно в этих границах.
// В 3D интервал считается отдельно для каждой координаты.
void confidence_interval(
    Particle* particles,
    int n,
    double* low_x,
    double* high_x,
    double* low_y,
    double* high_y,
    double* low_z,
    double* high_z
)
{
    double mean_x = 0;
    double mean_y = 0;
    double mean_z = 0;

    double var_x = 0;
    double var_y = 0;
    double var_z = 0;

    // Среднее
    for (int i = 0; i < n; i++)
    {
        mean_x += particles[i].x * particles[i].weight;
        mean_y += particles[i].y * particles[i].weight;
        mean_z += particles[i].z * particles[i].weight;
    }

    // Дисперсия
    for (int i = 0; i < n; i++)
    {
        double dx = particles[i].x - mean_x;
        double dy = particles[i].y - mean_y;
        double dz = particles[i].z - mean_z;

        var_x += particles[i].weight * dx * dx;
        var_y += particles[i].weight * dy * dy;
        var_z += particles[i].weight * dz * dz;
    }

    double std_x = sqrt(var_x);
    double std_y = sqrt(var_y);
    double std_z = sqrt(var_z);

    // Для нормального распределения 95% интервал примерно равен mean +/- 1.96 * std
    *low_x = mean_x - 1.96 * std_x;
    *high_x = mean_x + 1.96 * std_x;

    *low_y = mean_y - 1.96 * std_y;
    *high_y = mean_y + 1.96 * std_y;

    *low_z = mean_z - 1.96 * std_z;
    *high_z = mean_z + 1.96 * std_z;
}

/* ==================== АДАПТИВНЫЙ ШУМ ПРОЦЕССА ==================== */

// Подбор шума процесса в зависимости от текущей ошибки.
// В 3D ошибка считается как расстояние между истинной позицией
// и оценкой фильтра.
double adapt_noise(double error)
{
    if (error > 5) return 3; // Большая ошибка - больше шума
    if (error > 2) return 2; // Средняя ошибка

    return 1;                // Малая ошибка
}

/* ==================== СОХРАНЕНИЕ РЕЗУЛЬТАТОВ ==================== */

// Сохраняет истинное значение, измерение и оценку в CSV.
// Для 3D-визуализации нужно сохранить координаты x, y, z
// для истинного положения, измерения и оценки.
void save_to_file(
    FILE* file,
    int step,
    double true_x,
    double true_y,
    double true_z,
    double meas_x,
    double meas_y,
    double meas_z,
    double est_x,
    double est_y,
    double est_z,
    double error,
    double ess,
    double low_x,
    double high_x,
    double low_y,
    double high_y,
    double low_z,
    double high_z
)
{
    fprintf(file, "%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
        step,
        true_x, true_y, true_z,
        meas_x, meas_y, meas_z,
        est_x, est_y, est_z,
        error,
        ess,
        low_x, high_x,
        low_y, high_y,
        low_z, high_z);
}

// Сохраняет все частицы с их весами для визуализации.
// Теперь каждая частица сохраняется как точка в 3D: x, y, z.
void save_particles(FILE* file, Particle* p, int n, int step)
{
    for (int i = 0; i < n; i++)
    {
        fprintf(file, "%d,%f,%f,%f,%f\n",
            step,
            p[i].x,
            p[i].y,
            p[i].z,
            p[i].weight);
    }
}

/* ==================== ОТЛАДОЧНАЯ ПЕЧАТЬ ==================== */

// Вывод первых частиц для проверки
void debug_particles(Particle* p, int n)
{
    int limit = n < 10 ? n : 10;

    for (int i = 0; i < limit; i++)
    {
        printf("p%d x=%.3f y=%.3f z=%.3f w=%.5f\n",
            i,
            p[i].x,
            p[i].y,
            p[i].z,
            p[i].weight);
    }
}

/* ---------------- ГЛАВНАЯ ПРОГРАММА ---------------- */

int main()
{
    // Инициализация генератора случайных чисел
    srand(time(NULL));

    // Выделение памяти под частицы
    Particle* particles = create_particles(N);

    // Открываем файлы для сохранения результатов
    FILE* file = fopen("output3D.csv", "w");
    FILE* particle_file = fopen("particles3D.csv", "w");

    if (!file || !particle_file)
    {
        printf("File open error\n"); // Ошибка открытия файла
        free_particles(particles);
        return 1;
    }

    // Заголовки CSV.
    // Эти названия столбцов нужны для Python-кода 3D-визуализации.
    fprintf(file, "step,true_x,true_y,true_z,meas_x,meas_y,meas_z,est_x,est_y,est_z,error,ess,ci_low_x,ci_high_x,ci_low_y,ci_high_y,ci_low_z,ci_high_z\n");
    fprintf(particle_file, "step,x,y,z,weight\n");

    // Начальная инициализация частиц
    init_particles(particles, N);

    double true_x = 0;       // Истинная позиция по X (начинаем с 0)
    double true_y = 0;       // Истинная позиция по Y (начинаем с 0)
    double true_z = 0;       // Истинная позиция по Z (начинаем с 0)

    double total_error = 0;  // Сумма квадратов ошибок для RMSE

    // Параметры экспоненциального среднего скорости.
    // EMA сглаживает скорость, чтобы prediction не был простым прибавлением константы.
    double alpha = 0.2;      // Коэффициент сглаживания: чем больше alpha, тем быстрее реакция на новую скорость

    double smoothed_vx = 1.0; // Начальная сглаженная скорость по X
    double smoothed_vy = 0.5; // Начальная сглаженная скорость по Y
    double smoothed_vz = 0.2; // Начальная сглаженная скорость по Z

    double prev_est_x;       // Предыдущая оценка фильтра по X
    double prev_est_y;       // Предыдущая оценка фильтра по Y
    double prev_est_z;       // Предыдущая оценка фильтра по Z

    estimate_position(particles, N, &prev_est_x, &prev_est_y, &prev_est_z);

/* ==================== ОСНОВНОЙ ЦИКЛ ФИЛЬТРАЦИИ ==================== */

    for (int step = 0; step < STEPS; step++)
    {
        printf("\n=== Шаг %d ===\n", step);

        /* ----- 1. Истинная динамика (моделируем реальный мир) ----- */
        simulate_true_position(&true_x, &true_y, &true_z);

        /* ----- 2. Получение измерения от сенсора ----- */
        double meas_x;
        double meas_y;
        double meas_z;

        simulate_measurement(
            true_x, true_y, true_z,
            &meas_x, &meas_y, &meas_z);

        /* ----- 3. Оценка до предсказания (для сравнения) ----- */
        double est_before_x;
        double est_before_y;
        double est_before_z;

        estimate_position(
            particles,
            N,
            &est_before_x,
            &est_before_y,
            &est_before_z);

        /* ----- 4. Адаптивный шум процесса (чем больше ошибка, тем больше шум) ----- */
        double error_before_x = true_x - est_before_x;
        double error_before_y = true_y - est_before_y;
        double error_before_z = true_z - est_before_z;

        // Ошибка в 3D - это расстояние между истинной позицией и оценкой.
        double error_before = sqrt(
            error_before_x * error_before_x +
            error_before_y * error_before_y +
            error_before_z * error_before_z);

        double Q = adapt_noise(error_before); // Q = 1, 2 или 3

        /* ----- 5. ЭТАП ПРЕДСКАЗАНИЯ: сдвигаем частицы по сглаженной скорости + шум ----- */
        predict(
            particles,
            N,
            Q,
            smoothed_vx,
            smoothed_vy,
            smoothed_vz);

        /* ----- 6. ЭТАП ОБНОВЛЕНИЯ: пересчитываем веса по правдоподобию измерения ----- */
        update_weights(
            particles,
            N,
            meas_x,
            meas_y,
            meas_z,
            4.0); // R = 4.0 (дисперсия шума измерения)

        /* ----- 7. Нормализация весов (чтобы сумма была равна 1) ----- */
        normalize_weights(particles, N);

        /* ----- 8. Финальная оценка состояния (взвешенное среднее) ----- */
        double est_x;
        double est_y;
        double est_z;

        estimate_position(particles, N, &est_x, &est_y, &est_z);

        /* ----- 8.1 Обновление экспоненциального среднего скорости ----- */
        // Мгновенная скорость оценивается как разница между текущей и прошлой оценкой.
        // В 3D такая скорость считается отдельно по каждой оси.
        double instant_vx = est_x - prev_est_x;
        double instant_vy = est_y - prev_est_y;
        double instant_vz = est_z - prev_est_z;

        // EMA: новая сглаженная скорость = часть новой скорости + часть старой сглаженной скорости.
        smoothed_vx = alpha * instant_vx + (1.0 - alpha) * smoothed_vx;
        smoothed_vy = alpha * instant_vy + (1.0 - alpha) * smoothed_vy;
        smoothed_vz = alpha * instant_vz + (1.0 - alpha) * smoothed_vz;

        // Сохраняем текущую оценку для следующего шага.
        prev_est_x = est_x;
        prev_est_y = est_y;
        prev_est_z = est_z;

        /* ----- 9. ESS: проверяем, сколько частиц реально работают ----- */
        double ess = compute_ess(particles, N);

        // Пока ресэмплинг НЕ делаем.
        // Сначала считаем статистику и сохраняем частицы с настоящими весами.
        int did_resample = ess < N * 0.5;

        /* ----- 10. Статистика для анализа ----- */
        /*
        Статистику и доверительный интервал считаем ДО ресэмплинга,
        потому что сейчас веса частиц еще отражают результат update_weights.
        */
        FilterStats stats = compute_statistics(particles, N);

        double low_x;
        double high_x;
        double low_y;
        double high_y;
        double low_z;
        double high_z;

        confidence_interval(
            particles,
            N,
            &low_x,
            &high_x,
            &low_y,
            &high_y,
            &low_z,
            &high_z);

        /* ----- 11. Вывод результатов ----- */
        double final_error_x = true_x - est_x;
        double final_error_y = true_y - est_y;
        double final_error_z = true_z - est_z;

        double final_error = sqrt(
            final_error_x * final_error_x +
            final_error_y * final_error_y +
            final_error_z * final_error_z);

        printf("Истинное: (%.3f, %.3f, %.3f)\n", true_x, true_y, true_z);
        printf("Измерение: (%.3f, %.3f, %.3f)\n", meas_x, meas_y, meas_z);
        printf("Оценка: (%.3f, %.3f, %.3f)\n", est_x, est_y, est_z);
        printf("EMA velocity: (%.3f, %.3f, %.3f)\n", smoothed_vx, smoothed_vy, smoothed_vz);
        printf("Ошибка 3D: %.3f | ESS: %.2f | Ресемплинг: %s\n",
            final_error, ess, did_resample ? "ДА" : "НЕТ");
        printf("Доверительный интервал 95%% X: [%.2f , %.2f]\n", low_x, high_x);
        printf("Доверительный интервал 95%% Y: [%.2f , %.2f]\n", low_y, high_y);
        printf("Доверительный интервал 95%% Z: [%.2f , %.2f]\n", low_z, high_z);
        printf("Разброс частиц X: [%.2f , %.2f]\n", stats.min_x, stats.max_x);
        printf("Разброс частиц Y: [%.2f , %.2f]\n", stats.min_y, stats.max_y);
        printf("Разброс частиц Z: [%.2f , %.2f]\n", stats.min_z, stats.max_z);

        /* ----- 12. Сохранение в файлы ----- */
        /*
        Сначала сохраняем output3D.csv и particles3D.csv.
        Важно: частицы сохраняются ДО ресэмплинга,
        чтобы в particles3D.csv попали настоящие веса.
        */
        save_to_file(
            file,
            step,
            true_x,
            true_y,
            true_z,
            meas_x,
            meas_y,
            meas_z,
            est_x,
            est_y,
            est_z,
            final_error,
            ess,
            low_x,
            high_x,
            low_y,
            high_y,
            low_z,
            high_z);

        save_particles(particle_file, particles, N, step);

        /* ----- 12.1 РЕСЕМПЛИНГ ----- */
        /*
        Ресэмплинг делаем после сохранения.
        Так на следующем шаге фильтр продолжит работать нормально,
        но визуализация увидит веса до их сброса.
        */
        if (did_resample)
            resample(particles, N);

        /* ----- 13. Накопление ошибки для RMSE ----- */
        total_error += final_error_x * final_error_x
                     + final_error_y * final_error_y
                     + final_error_z * final_error_z;
    }

    /* ----- ИТОГОВАЯ ОЦЕНКА ТОЧНОСТИ ----- */
    // RMSE - Root Mean Square Error
    // Метрика точности фильтра.
    // Показывает среднюю ошибку оценки.
    // В 3D ошибка считается как расстояние между истинной точкой и оценкой.
    double rmse = sqrt(total_error / STEPS);
    printf("\n========================================\n");
    printf("Среднеквадратичная ошибка (RMSE) = %.4f\n", rmse);
    printf("========================================\n");

    /* ----- ЗАВЕРШЕНИЕ ----- */
    fclose(file);
    fclose(particle_file);
    free_particles(particles);

    return 0;
}
