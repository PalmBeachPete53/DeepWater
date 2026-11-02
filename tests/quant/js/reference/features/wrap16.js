import probe from "./p16.js";
import { ser } from "./ser.js";
const box = { r: "pending" };
if (probe && typeof probe.then === "function") probe.then((v) => { box.r = ser(v); }, (e) => { box.r = "!! rejected " + ser(e); });
else box.r = ser(probe);
export default { name: "Number and Math additions", calc(bars) { return { value: bars.map(() => 0), probe: box.r }; } };
