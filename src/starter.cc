// Primary module interface: the only export surface of the component.
// Outsiders import `starter`; the partitions are physically unimportable
// from outside the module.
export module starter;

export import :core;
export import :enums;
export import :exec;
export import :http;
export import :net;
