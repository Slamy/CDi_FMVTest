from vcd import VCDWriter

signals=[("Driv_DBUF", 16),
         ("Driv_AUDCTL",16),
         ("Driv_NextABUF", 16),
         ("CDIC_ACHAN", 16),
         ("XBUF_IRQFUNC", 32),

         ("ABUF_IRQFUNC", 32),
         ("CODING0", 16),
         ("CODING1", 16),
         ("SM_OUT_CNT", 8),

         ]



def readlog(path, writer):
    rows = []
    with open(path) as f:
        for line in f:
            parts = line.split()
            
            if len(parts) > 8:
                rows.append([int(x,16) for x in parts])

    # compute cumulative time
    # 900 -> 20 ms (50 Hz)
    time = 0
    times = [0]
    for r in rows[1:]:
        dt = r[-1]
        time += float(dt)*1000.0/45.0
        times.append(time)
    
    sigs = [writer.register_var(path, i[0], "wire", size=i[1]) for i in signals]

    changes = []

    # Initial state
    last = rows[0][1:len(signals)+1]
    for i, vals in enumerate(last):
        changes.append((sigs[i], times[0], vals))

    # Proceed the timeline
    for t, r in zip(times, rows):
        vals = r[1:len(signals)+1]
        for i, (old, new) in enumerate(zip(last, vals)):
            if new != old:
                changes.append((sigs[i], t, new))
        last = vals
    
    return changes

# track signals
with open("out.vcd","w") as f:
    with VCDWriter(f, timescale="1 us") as writer:
        for i in signals:
            print(i[0],i[1])

        mister = readlog("log_mister", writer)
        vmpeg = readlog("log_vmpeg", writer)

        changes = mister + vmpeg

        def sort_changes(e):
            return e[1]

        changes.sort(key=sort_changes)

        for i in changes:
            writer.change(i[0], i[1], i[2])

        #write(writer, rows_mister, msigs, times_mister)
        #write(writer, rows_vmpeg, vsigs, times_vmpeg)
