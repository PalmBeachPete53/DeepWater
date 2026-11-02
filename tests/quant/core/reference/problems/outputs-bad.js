export default { name: 'Ok',
  inputs: [{ key: 'length', type: 'int', default: 14 }],
  outputs: [{ key: 'value', kind: 'line', color: '#2962ff' }],
  calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), p.length) }), outputs: [{ key: 'value', color: '  ' }, { key: 'w', lineWidth: 5 }, { key: 'x', lineWidth: 1.5 }, null, { key: 'y', color: 3 }] };