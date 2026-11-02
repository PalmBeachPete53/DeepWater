import probe from "./p94.js";
import { ser } from "./ser.js";
const box = { r: "pending" };
if (probe && typeof probe.then === "function") probe.then((v) => { box.r = ser(v); }, (e) => { box.r = "!! rejected " + ser(e); });
else box.r = ser(probe);
export default { name: "rest of the same object is a fresh copy", calc(bars) { return { value: bars.map(() => 0), probe: box.r }; } };
