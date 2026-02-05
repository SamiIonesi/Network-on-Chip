import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def generate_reports():
    try:
        df = pd.read_csv('performance_report.csv')
    except:
        print("Error: performance_report.csv not found!")
        return

    # Filter Routers and CPUs
    routers = df[df['Entity'].str.contains('Router')].copy()
    cpus = df[df['Entity'].str.contains('CPU')].copy()

    # 1. Bar Chart: Traffic and Drops
    plt.figure(figsize=(12, 6))
    x = routers['Entity']
    plt.bar(x, routers['Routed'], label='Routed Packets', color='skyblue')
    plt.bar(x, routers['TTLDrops'], label='TTL Drops (Loops)', color='red', alpha=0.7)
    plt.xticks(rotation=90)
    plt.title('Router Traffic and TTL Expirations')
    plt.ylabel('Count')
    plt.legend()
    plt.tight_layout()
    plt.savefig('traffic_report.png')

    # 2. Heatmap: Routed Packets in 6x6 Grid
    plt.figure(figsize=(8, 6))
    # Extract numeric ID from 'Router_X'
    routers['id'] = routers['Entity'].str.extract('(\d+)').astype(int)
    grid_data = routers.sort_values('id')['Routed'].values.reshape(6, 6)
    
    sns.heatmap(grid_data, annot=True, cmap='YlGnBu', fmt='g')
    plt.title('Traffic Intensity Heatmap (6x6 Torus)')
    plt.savefig('heatmap.png')

    # 3. CPU Latency Report
    if not cpus.empty:
        plt.figure(figsize=(8, 5))
        sns.barplot(x='Entity', y='AvgLatency_ns', data=cpus)
        plt.title('Average End-to-End Latency per CPU')
        plt.ylabel('Latency (ns)')
        plt.savefig('latency_report.png')

    print("Reports generated: traffic_report.png, heatmap.png, latency_report.png")

if __name__ == "__main__":
    generate_reports()