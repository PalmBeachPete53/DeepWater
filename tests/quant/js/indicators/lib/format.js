// A helper module of template-markers.js: object spread and logical assignment here too.
const cache = {};
export const defaults = { color: "#26a69a", ...{ shape: "circle" } };
export function describe(text, { max = 40, ...opts } = {}) {
  cache[text] ??= `${text.length > max ? `${text.slice(0, max)}...` : text}${opts.suffix ?? ""}`;
  return cache[text];
}
