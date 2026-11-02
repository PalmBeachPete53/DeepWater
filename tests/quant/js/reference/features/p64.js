const v = await Promise.resolve(5);
export default [v, await (async () => 6)()];
