// Position signal imported from test.csv on 2026-10-29.
// +1 long, 0 flat, -1 short, each holding until the next change. Import the CSV again to replace it.
const T = [1706313600,1708909200,1708992000,1731369600,1769738400,1769918460];
const P = [1,0,-1,1,-1,1];

export default {
  name: "Signal: TEST",
  overlay: false,
  inputs: [],
  outputs: [{ key: "position", label: "Position", kind: "histogram", color: "#2962ff" }],
  levels: [0],
  calc(bars) {
    const position = new Array(bars.length).fill(NaN);
    const colors = new Array(bars.length).fill("#787b86");
    let k = -1;
    for (let i = 0; i < bars.length; i++) {
      const end = i + 1 < bars.length ? bars[i + 1].time : bars[i].time + (i ? bars[i].time - bars[i - 1].time : 86400);
      while (k + 1 < T.length && T[k + 1] < end) k++;
      if (k < 0) continue;
      position[i] = P[k];
      colors[i] = P[k] > 0 ? "#089981" : P[k] < 0 ? "#f23645" : "#787b86";
    }
    return { position, colors };
  },
};
