import probe from "./p95.js";
import { ser } from "./ser.js";
const box = { r: "pending" };
if (probe && typeof probe.then === "function") probe.then((v) => { box.r = ser(v); }, (e) => { box.r = "!! rejected " + ser(e); });
else box.r = ser(probe);
export default { name: "logical assignment evaluates the target once", calc(bars) { return { value: bars.map(() => 0), probe: box.r }; } };
