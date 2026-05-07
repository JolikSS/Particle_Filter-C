import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv("output.csv")

plt.figure(figsize=(10, 6))

plt.plot(data["true"], label="True Position")
plt.plot(data["measurement"], label="Measurement", linestyle="dotted")
plt.plot(data["estimate"], label="Estimate (Particle Filter)")

plt.legend()
plt.xlabel("Step")
plt.ylabel("Position")
plt.title("Particle Filter Visualization")
plt.grid()
plt.tight_layout()
plt.show()