export module Core.STLExtension;

export import Core.Prelude;

template<typename Transform>
struct ThenData {
    Transform transform;
};

export template<typename T, typename Transform>
auto operator|(std::future<T> future, ThenData<Transform> thenData) {
    return std::async(std::launch::async, [f = std::move(future), t = std::move(thenData.transform)]() mutable {
        return t(f.get());
    });
}

export template<typename Transform>
ThenData<Transform> Then(Transform transform) {
    return ThenData<Transform>{std::move(transform)};
}
