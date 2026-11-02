export default (function () {
return [Number.isInteger(5.0), Number.isSafeInteger(2 ** 53), Number.EPSILON > 0, Number.MAX_SAFE_INTEGER, Number.parseFloat("1.5x"),
Number.isNaN("x"), Math.trunc(-4.7), Math.sign(-3), Math.cbrt(27), Math.log10(1000), Math.log2(8), Math.hypot(3, 4), Math.fround(5.5), Math.clz32(1),
Math.imul(3, 4), Math.expm1(0), Math.log1p(0), Math.sinh(0), Math.cosh(0), Math.tanh(0), Math.asinh(0), Math.acosh(1), Math.atanh(0)];
})();
