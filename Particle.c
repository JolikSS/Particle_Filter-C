#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 500         // Количество частиц (гипотез о положении объекта).
                      // Чем больше частиц — тем точнее оценка,
                      // но тем больше вычислений.

#define STEPS 50      // Количество шагов симуляции (итераций фильтра).
                      // На каждом шаге объект двигается,
                      // получаем измерение и обновляем фильтр.

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Одна частица представляет гипотезу о положении объекта.
// Particle Filter хранит много таких гипотез одновременно.
typedef struct
{
    double x;       // Предполагаемая позиция объекта(где, по мнению этой частицы, находится объект)

    double weight;  // Вес частицы = вероятность того,
                    // что именно эта гипотеза ближе всего к реальности.
                    // Чем ближе частица к измерению — тем больше вес.
} Particle;

// Статистика распределения частиц.
// Используется для анализа работы фильтра.
typedef struct
{
    double mean;       // Среднее значение позиции частиц
                       // (это приблизительная оценка положения объекта)

    double variance;   // Дисперсия — показывает разброс частиц
                       // вокруг среднего значения.
                       // Маленькая дисперсия - фильтр уверен.
                       // Большая дисперсия - фильтр не уверен.

    double min;        // Минимальное значение позиции частицы
                       // показывает левую границу распределения

    double max;        // Максимальное значение позиции частицы
                       // показывает правую границу распределения
} FilterStats;

// Генерация случайного числа в диапазоне (0,1)
double rand_uniform()
{
    return ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
}

// Генерация случайного числа с нормальным распределением.
// mean   — среднее значение
// stddev — стандартное отклонение
// Используется преобразование Бокса-Мюллера, которое превращает два равномерных числа в одно нормально распределённое.
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
double simulate_true_position(double x)
{
    double velocity = 1.0;              // постоянная скорость объекта
    double noise = rand_normal(0, 0.5); // случайный шум движения

    // реальная позиция = старая позиция + скорость + случайное отклонение
    return x + velocity + noise;
}

// Модель сенсора
// Сенсор не видит точную позицию, а возвращает значение с шумом.
double simulate_measurement(double true_x)
{
    return true_x + rand_normal(0, 2.0);
}

/* ==================== ИНИЦИАЛИЗАЦИЯ ФИЛЬТРА ==================== */
// Начальное распределение частиц.
// Мы ничего не знаем о позиции объекта, поэтому распределяем частицы равномерно.
void init_particles(Particle* particles, int n)
{
    for (int i = 0; i < n; i++)
    {
        // случайная позиция от -10 до 10
        particles[i].x = -10 + 20 * rand_uniform();

        // все частицы одинаково вероятны
        particles[i].weight = 1.0 / n;
    }
}
/* ---------------- ПРЕДСКАЗАНИЕ ---------------- */
// Мы предполагаем, что частицы движутся так же, как и реальный объект (модель движения).
// К каждой частице добавляется скорость + случайный шум.
void predict(Particle* particles, int n, double Q)
{
    double velocity = 1.0;

    for (int i = 0; i < n; i++)
    {
        particles[i].x += velocity + rand_normal(0, sqrt(Q));
    }
}

/* ---------------- ОБНОВЛЕНИЕ ВЕСОВ ---------------- */
// Пересчитываем веса частиц на основе измерения.
// Если частица близко к измерению, её вес становится большим. Если далеко — маленьким.
void update_weights(Particle* particles, int n, double z, double R)
{
    for (int i = 0; i < n; i++)
    {
        double diff = z - particles[i].x;

        // формула гауссовского правдоподобия
        double exponent = -(diff * diff) / (2.0 * R);

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
double estimate_position(Particle* particles, int n)
{
    double est = 0;

    for (int i = 0; i < n; i++)
        est += particles[i].x * particles[i].weight;

    return est;
}

/* ---------------- СТАТИСТИКА ---------------- */

// Вычисление среднего, дисперсии, минимума и максимума по частицам
FilterStats compute_statistics(Particle* particles, int n)
{
    FilterStats stats;

    stats.mean = 0;
    stats.variance = 0;

    stats.min = particles[0].x;
    stats.max = particles[0].x;

    // Среднее и границы
    for (int i = 0; i < n; i++)
    {
        stats.mean += particles[i].x * particles[i].weight;

        if (particles[i].x < stats.min)
            stats.min = particles[i].x;

        if (particles[i].x > stats.max)
            stats.max = particles[i].x;
    }
    
    // Дисперсия
    for (int i = 0; i < n; i++)
    {
        double diff = particles[i].x - stats.mean;
        stats.variance += particles[i].weight * diff * diff;
    }

    return stats;
}

/* ---------------- ДОВЕРИТЕЛЬНЫЙ ИНТЕРВАЛ ---------------- */

// Приближённый доверительный интервал: mean ± 2 * std
void confidence_interval(Particle* particles, int n, double* low, double* high)
{
    double mean = 0;
    double var = 0;

    // Среднее
    for (int i = 0; i < n; i++)
        mean += particles[i].x * particles[i].weight;

    // Дисперсия
    for (int i = 0; i < n; i++)
    {
        double d = particles[i].x - mean;
        var += particles[i].weight * d * d;
    }

    double std = sqrt(var);

    *low = mean - 2 * std;  // Нижняя граница
    *high = mean + 2 * std; // Верхняя граница
}

/* ==================== АДАПТИВНЫЙ ШУМ ПРОЦЕССА ==================== */

// Подбор шума процесса в зависимости от текущей ошибки
double adapt_noise(double error)
{
    if (error > 5) return 3; // Большая ошибка — больше шума
    if (error > 2) return 2; // Средняя ошибка

    return 1;                // Малая ошибка
}

/* ==================== СОХРАНЕНИЕ РЕЗУЛЬТАТОВ ==================== */

// Сохраняет истинное значение, измерение и оценку в CSV
void save_to_file(FILE* file, double true_x, double meas, double est)
{
    fprintf(file, "%f,%f,%f\n", true_x, meas, est);
}

// Сохраняет все частицы с их весами для визуализации
void save_particles(FILE* file, Particle* p, int n, int step)
{
    for (int i = 0; i < n; i++)
        fprintf(file, "%d,%f,%f\n", step, p[i].x, p[i].weight);
}

/* ==================== ОТЛАДОЧНАЯ ПЕЧАТЬ ==================== */

// Вывод первых частиц для проверки
void debug_particles(Particle* p, int n)
{
    int limit = n < 10 ? n : 10;

    for (int i = 0; i < limit; i++)
    {
        printf("p%d x=%.3f w=%.5f\n", i, p[i].x, p[i].weight);
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
    FILE* file = fopen("output.csv", "w");
    FILE* particle_file = fopen("particles.csv", "w");

    if (!file || !particle_file)
    {
        printf("File open error\n"); // Ошибка открытия файла
        free_particles(particles);
        return 1;
    }

    // Заголовки CSV
    fprintf(file, "true,measurement,estimate\n");

    // Начальная инициализация частиц
    init_particles(particles, N);

    double true_x = 0;       // Истинная позиция (начинаем с 0)
    double total_error = 0;  // Сумма квадратов ошибок для RMSE

/* ==================== ОСНОВНОЙ ЦИКЛ ФИЛЬТРАЦИИ ==================== */

    for (int step = 0; step < STEPS; step++)
    {
        printf("\n=== Шаг %d ===\n", step);
        
        /* ----- 1. Истинная динамика (моделируем реальный мир) ----- */
        true_x = simulate_true_position(true_x);
        
        /* ----- 2. Получение измерения от сенсора ----- */
        double z = simulate_measurement(true_x);
        
        /* ----- 3. Оценка до предсказания (для сравнения) ----- */
        double est_before = estimate_position(particles, N);
        
        /* ----- 4. Адаптивный шум процесса (чем больше ошибка, тем больше шум) ----- */
        double error = fabs(true_x - est_before);
        double Q = adapt_noise(error);  // Q = 1, 2 или 3
        
        /* ----- 5. ЭТАП ПРЕДСКАЗАНИЯ: сдвигаем частицы по модели движения + шум ----- */
        predict(particles, N, Q);
        
        /* ----- 6. ЭТАП ОБНОВЛЕНИЯ: пересчитываем веса по правдоподобию измерения ----- */
        update_weights(particles, N, z, 4.0);  // R = 4.0 (дисперсия шума измерения)
        
        /* ----- 7. Нормализация весов (чтобы сумма была равна 1) ----- */
        normalize_weights(particles, N);
        
        /* ----- 8. Финальная оценка состояния (взвешенное среднее) ----- */
        double est = estimate_position(particles, N);
        
        /* ----- 9. РЕСЕМПЛИНГ: если ESS < 50% от N, удаляем частицы с малыми весами ----- */
        double ess = compute_ess(particles, N);
        if (ess < N * 0.5)
            resample(particles, N);
        
        /* ----- 10. Статистика для анализа ----- */
        FilterStats stats = compute_statistics(particles, N);
        double low, high;
        confidence_interval(particles, N, &low, &high);
        
        /* ----- 11. Вывод результатов ----- */
        printf("Истинное: %.3f | Измерение: %.3f | Оценка: %.3f\n", true_x, z, est);
        printf("Ошибка: %.3f | ESS: %.2f | Ресемплинг: %s\n", 
            fabs(true_x - est), ess, (ess < N*0.5) ? "ДА" : "НЕТ");
        printf("Доверительный интервал 95%%: [%.2f , %.2f]\n", low, high);
        printf("Разброс частиц: [%.2f , %.2f]\n", stats.min, stats.max);
        
        /* ----- 12. Сохранение в файлы ----- */
        save_to_file(file, true_x, z, est);
        save_particles(particle_file, particles, N, step);
        
        /* ----- 13. Накопление ошибки для RMSE ----- */
        total_error += (true_x - est) * (true_x - est);
    }

    /* ----- ИТОГОВАЯ ОЦЕНКА ТОЧНОСТИ ----- */
    // RMSE — Root Mean Square Error
    // Метрика точности фильтра.
    // Показывает среднюю ошибку оценки.
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