export * as helpers from "./probe-helper.js";
import * as self from "./probe-helper.js";
export default [typeof self.helper, self.helper(3)];
