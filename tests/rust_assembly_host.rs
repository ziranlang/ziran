use ziran_generated as api;

fn expected_marker() -> i32 {
    if cfg!(ziran_no_asm) {
        return 3;
    }
    #[cfg(target_arch = "x86_64")]
    {
        return if std::arch::is_x86_feature_detected!("popcnt") { 11 } else { 7 };
    }
    #[cfg(any(target_arch = "aarch64", target_arch = "x86"))]
    {
        return 7;
    }
    #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64", target_arch = "x86")))]
    {
        3
    }
}

fn main() {
    let _: extern "C" fn(usize) -> usize = api::Word;
    let _: extern "C" fn(isize) -> isize = api::SignedWord;
    assert_eq!(api::Word(usize::MAX), 0);
    assert_eq!(api::SignedWord(-1), -1);
    let expected = expected_marker();
    // WASI preview1 has no threads; native runs exercise cold shared dispatch.
    #[cfg(not(target_os = "wasi"))]
    {
        let workers: Vec<_> = (0..4).map(|_| std::thread::spawn(move || {
            for _ in 0..100 {
                assert_eq!(api::Marker(), expected);
                assert_eq!(api::Count(u64::MAX), 64);
            }
        })).collect();
        for worker in workers { worker.join().unwrap(); }
    }
    assert_eq!(api::Marker(), expected);
    let mut state = 0x123456789abcdefu64;
    for i in 0..200u64 {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1);
        assert_eq!(api::Count(state), state.count_ones() as u64);
        assert_eq!(api::Count32(state as u32), (state as u32).count_ones());
        assert_eq!(api::Byte(state as i8), state as i8);
        assert_eq!(api::Narrow(state), state as u32);
        assert_eq!(api::FixedDestination(state), state as u32);
        assert_eq!(api::Odd(state), state & 1 != 0);
        assert_eq!(api::BlockOdd(state), state & 1 != 0);
        assert_eq!(api::BoolValue(i & 1 != 0), (i & 1) as u32);
        assert_eq!(api::Add(i as i64), i as i64 + 7);
        assert_eq!(api::NamedTie(i as i64), i as i64 + 7);
        let parts = api::Parts(state);
        assert_eq!((parts.low, parts.high), (state, state >> 32));
        let fields = api::KeywordFields(state);
        assert_eq!((fields.ziran_keyword_type_1, fields.ziran_keyword_type_0), (state, state));
        let parts = api::MemParts(state);
        assert_eq!((parts.low, parts.high), (state, state >> 32));
        assert_eq!(api::MemInput(state), state);
        let divisor = i + 1;
        let parts = api::DivParts(state, divisor);
        assert_eq!((parts.low, parts.high), (state / divisor, state % divisor));
        assert_eq!(api::Mixed(state), 0);
        assert_eq!(api::Memory(i as i64), i as i64 * 2);
        assert_eq!(api::FloatAdd(i as f64 * 0.5, 1.5), i as f64 * 0.5 + 1.5);
        assert_eq!(api::Shadow(i as i64, 7), i as i64 + 7);
        assert_eq!(api::Dialect(i as i64), i as i64);
        assert_eq!(api::DialectSuffix(i as i64), i as i64);
        assert_eq!(api::Labels(i & 31), (i & 31) * ((i & 31) + 1) / 2);
        assert_eq!(api::LabelData(), 48);
        let mut value = i as i64;
        api::Store(&mut value);
        assert_eq!(value, i as i64 + 1);
        api::Barrier();
    }
    assert!(api::Odd(1));
    assert!(!api::Odd(0));
    println!("Rust assembly conformance passed: marker={}", expected);
}
