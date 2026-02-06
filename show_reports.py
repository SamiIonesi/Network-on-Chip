import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def generate_full_report():
    try:
        df = pd.read_csv('performance_report.csv')
    except FileNotFoundError:
        print("❌ ERROR: 'performance_report.csv' not found!")
        print("   Run the C++ simulation first to generate this file.")
        return

    # Filter Routers and CPUs
    routers = df[df['Entity'].str.contains('Router')].copy()
    cpus = df[df['Entity'].str.contains('CPU')].copy()

    # Extract numeric ID for sorting
    routers['id'] = routers['Entity'].str.extract(r'(\d+)').astype(int)
    routers = routers.sort_values('id')
    cpus['id'] = cpus['Entity'].str.extract(r'(\d+)').astype(int)

    # =========================================================
    # NEW CALCULATIONS (SENT, SUCCESS, DROPS)
    # =========================================================
    
    # 1. SUCCESS: Packets received by CPUs (column 'Routed' for CPUs = received_packets from C++)
    total_success = cpus['Routed'].sum()

    # 2. DROPS: Sum of drops across all routers
    # Check if columns exist (for compatibility)
    drop_ttl = routers['TTLDrops'].sum()
    drop_no_route = routers['NoRouteDrops'].sum() if 'NoRouteDrops' in df.columns else 0
    drop_disabled = routers['DisabledDrops'].sum() if 'DisabledDrops' in df.columns else 0
    
    total_drops = drop_ttl + drop_no_route + drop_disabled

    # 3. TOTAL SENT: Success + All Drops (Packets that had a definitive outcome)
    # Note: Does not include packets still "in-flight" at simulation end.
    total_sent = total_success + total_drops

    # =========================================================
    # TEXTUAL REPORT
    # =========================================================
    print("\n" + "="*60)
    print(" 📊  FULL NoC PERFORMANCE REPORT (6x4 TORUS)")
    print("="*60)

    # --- Router Stats ---
    total_hops = routers['Routed'].sum() # Here 'Routed' for Routers means HOPS
    total_reroutes = routers['Reroutes'].sum()
    most_active = routers.loc[routers['Routed'].idxmax()]

    print(f"\n[1] GLOBAL TRAFFIC SUMMARY")
    print(f"  - 📦 Total Packets Sent (Est.): {int(total_sent)}")
    print(f"  - ✅ Successful Deliveries:     {int(total_success)}")
    print(f"  - ❌ LOST PACKETS (Total Drop): {int(total_drops)}")
    print(f"      ├─ ⏳ TTL Expired:           {drop_ttl}")
    print(f"      ├─ 🚫 No Destination (Route):{drop_no_route}")
    print(f"      └─ 🔒 Port Disabled:         {drop_disabled}")
    
    print(f"\n[2] NETWORK ACTIVITY (ROUTERS)")
    print(f"  - Total Hops (Switching):        {total_hops}")
    print(f"  - Adaptive Reroutes:             {total_reroutes}")
    print(f"  - Hotspot Node:                  {most_active['Entity']} ({most_active['Routed']} hops)")

    # --- CPU Stats ---
    active_cpus = cpus[cpus['AvgLatency_ns'] > 0]
    if not active_cpus.empty:
        avg_lat = active_cpus['AvgLatency_ns'].mean()
        max_lat_cpu = active_cpus.loc[active_cpus['AvgLatency_ns'].idxmax()]
        
        print(f"\n[3] LATENCY PERFORMANCE (CPU)")
        print(f"  - Average Global Latency:        {avg_lat:.2f} ns")
        print(f"  - Slowest Path:                  {max_lat_cpu['Entity']} ({max_lat_cpu['AvgLatency_ns']:.2f} ns)")
    else:
        print("\n[3] CPU PERFORMANCE: No packets successfully delivered.")

    print("\n" + "="*60)

    # =========================================================
    # GRAPH GENERATION
    # =========================================================
    sns.set_theme(style="whitegrid")

    # A. Heatmap
    plt.figure(figsize=(10, 6))
    try:
        grid_data = routers['Routed'].values.reshape(4, 6)
        sns.heatmap(grid_data, annot=True, cmap='YlOrRd', fmt='g', cbar_kws={'label': 'Hops'})
        plt.title('Traffic Intensity Heatmap (6x4 Torus)', fontsize=14)
        plt.savefig('report_heatmap.png')
        print("Saved: report_heatmap.png")
    except:
        pass

    # B. Latency
    if not active_cpus.empty:
        plt.figure(figsize=(10, 5))
        sns.barplot(x='Entity', y='AvgLatency_ns', data=cpus, palette='viridis', hue='Entity', legend=False)
        plt.title('Average Latency per CPU', fontsize=14)
        plt.savefig('report_latency.png')
        print("Saved: report_latency.png")

    # C. Detailed Drops Chart
    plt.figure(figsize=(12, 6))
    x = routers['Entity'].str.replace('Router_', 'R')
    
    # Stacked Bar Chart for Drops
    p1 = plt.bar(x, routers['Routed'], label='Routed (Success)', color='#a8dadc')
    
    # Ensure columns exist in DataFrame
    ttl_d = routers['TTLDrops'] if 'TTLDrops' in routers else 0
    no_route_d = routers['NoRouteDrops'] if 'NoRouteDrops' in routers else 0
    disabled_d = routers['DisabledDrops'] if 'DisabledDrops' in routers else 0

    p2 = plt.bar(x, ttl_d, bottom=routers['Routed'], label='TTL Drop', color='#e63946')
    p3 = plt.bar(x, no_route_d, bottom=routers['Routed']+ttl_d, label='No Route Drop', color='#457b9d')
    # p4 Disabled drops (optional, usually 0)
    
    plt.title('Router Activity: Forwarding vs. Drops', fontsize=14)
    plt.legend()
    plt.xticks(rotation=90)
    plt.tight_layout()
    plt.savefig('report_traffic_drops.png')
    print("Saved: report_traffic_drops.png")

if __name__ == "__main__":
    generate_full_report()