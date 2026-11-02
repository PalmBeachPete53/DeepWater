export default { name: 'Ok',
  inputs: [{ key: 'length', type: 'int', default: 14 }],
  outputs: [{ key: 'value', kind: 'line', color: '#2962ff' }],
  calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), p.length) }), inputs: [{ key: '1x', type: 'int', default: 1 }, { type: 'int', default: 1 }, null, 5, { key: 'a', type: 'float', default: 'x' }, { key: 'a', type: 'float', default: Infinity }, { key: 'b', type: 'time', default: -1 }, { key: 'c', type: 'time', default: 0, min: 'x', max: NaN }, { key: 'd', type: 'int', default: 3, min: 1, max: 9 }] };