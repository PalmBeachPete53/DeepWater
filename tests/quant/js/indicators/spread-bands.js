// Object spread and rest around ta.compute results, rest in helper parameters, numeric separators.
const pick = ({ upper, lower, ...rest }) => ({ upper, lower, extra: Object.keys(rest).sort().join("+") });
const merge = (...parts) => parts.reduce((acc, part) => ({ ...acc, ...part }), {});
const SCALE = 1_000;

export default {
  name: "Spread bands",
  overlay: true,
  inputs: [
    { key: "length", label: "Length", type: "int", default: 20, min: 2 },
    { key: "mult", label: "Mult", type: "float", default: 2 },
    { key: "source", label: "Source", type: "source", default: "close" },
  ],
  outputs: [
    { key: "upper", color: "#2962ff" },
    { key: "lower", color: "#2962ff" },
    { key: "mid", color: "#ff9800" },
    { key: "width", kind: "histogram", color: "#26a69a" },
  ],
  calc(bars, { length, mult, ...rest }, ta) {
    const bb = { ...ta.compute("bb", bars, { length, mult, source: rest.source }), kind: "bb" };
    const kc = { ...ta.compute("kc", bars, { length, mult }) };
    const { upper, lower, extra } = pick(bb);
    const { basis: mid, ...kcBands } = kc;
    const all = merge({ upper }, { lower }, { mid }, { kcUpper: kcBands.upper }, null, undefined);
    const width = all.upper.map((u, i) => {
      const { [i]: lo = NaN } = all.lower;
      return Number.isFinite(u) && Number.isFinite(lo) ? ((u - lo) / all.mid[i]) * SCALE : NaN;
    });
    return { ...all, width, info: { extra, keys: Object.keys({ ...kcBands, ...rest }) } };
  },
};
