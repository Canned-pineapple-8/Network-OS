import matplotlib.pyplot as plt

# читаем данные из файла
cabins = []
avg_waits = []

with open("bathroom_stats.txt") as f:
    for line in f:
        if line.strip():
            c, w = line.strip().split()
            cabins.append(int(c))
            avg_waits.append(float(w))

plt.figure(figsize=(10,6))
plt.plot(cabins, avg_waits, marker='o')
plt.title("Среднее время ожидания студента в очереди vs количество кабинок")
plt.xlabel("Количество кабинок")
plt.ylabel("Среднее время ожидания (сек)")
plt.grid(True)
plt.xticks(cabins)
plt.show()
