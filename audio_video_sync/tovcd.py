from vcd import VCDWriter

# parse your table
rows = []
with open("log_vmpeg") as f:
    for line in f:
        parts = line.split()
        rows.append([int(x,16) for x in parts])

# compute cumulative time
time = 0
times = [0]
for r in rows[1:]:
    dt = r[-1]
    time += float(dt)*1000.0/45.0
    times.append(time)

signals=[("dts", 32),
         ("FMV_PICS_IN_FIFO", 8),
         ("V_BufStat", 8),
         ("FMA_Sig", 16),
         ("FMV_Sig", 16),
         ("FMV_IMGSZ", 32),
         ("FMV_PICSZ", 32),
         ]


# track signals
with open("out.vcd","w") as f:
    with VCDWriter(f, timescale="1 us") as writer:

        for i in signals:
            print(i[0],i[1])

        sigs = [writer.register_var("top", i[0], "wire", size=i[1]) for i in signals]

        last = rows[0][1:8]

        for i, vals in enumerate(last):
            writer.change(sigs[i], times[0], vals)

        for t, r in zip(times, rows):
            vals = r[1:8]
            for i, (old, new) in enumerate(zip(last, vals)):
                if new != old:
                    writer.change(sigs[i], t, new)
            last = vals
