# summarize_latency_per_line.py

import pandas as pd
from io import StringIO

data = """
Scheme	NodeNum	ttime	tops	ops	tsize	thruput	p95	p99
AC-Cache	24	776.932	2000000	2586.23	2000000	2.52561	0.020164	0.029702
AC-Cache	24	746.229	2000000	2690.67	2000000	2.62761	0.019345	0.028109
AC-Cache	24	1308.63	2000000	1538.3	2000000	1.50225	0.018656	0.02569
ECCache	24	934.506	2000000	2143.6	2000000	2.09336	0.0352129	0.054839
ECCache	24	959.619	2000000	2087.86	2000000	2.03893	0.0365782	0.0595315
ECCache	24	1135.07	2000000	1764.14	2000000	1.72279	0.0498353	0.085056
Memcached	24	1008.75	2000000	1985.22	2000000	1.93869	0.030493	0.039789
Memcached	24	1963.92	2000000	1021.07	1997698	0.99599	0.045861	0.084307
Memcached	24	1046.76	2000000	1912.83	2000000	1.868	0.031608	0.04171
SPCache	24	1008.79	2000000	1986.82	2000000	1.94026	0.020321	0.03014
SPCache	24	1059.12	2000000	1892.73	2000000	1.84837	0.021771	0.032828
SPCache	24	1066.56	2000000	1879.15	2000000	1.8351	0.021899	0.032827
"""

df = pd.read_csv(StringIO(data), sep="\t")

for _, row in df.iterrows():
    print(f"=============={row['Scheme']}==============")
    print(f"Total time: {row['ttime']:.2f}")
    print(f"Total ops: {int(row['tops'])}")
    print(f"Total op throughput: {row['ops']:.2f}")
    print(f"Total sizes: {int(row['tsize'])}")
    print(f"Total size throughput: {row['thruput']:.5f}")
    print(f"95% latency: {row['p95'] * 1000:.3f}")
    print(f"99% latency: {row['p99'] * 1000:.3f}\n")