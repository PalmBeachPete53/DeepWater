const base = { name: 'Spread ok' };
export default { ...base, calc(bars) { return { value: bars.map(({ close, ...rest }) => close + Object.keys(rest).length) }; } };
