#include <gtest/gtest.h>
#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/parser/parser.hpp>
#include <cstdlib>
#include <filesystem>

using namespace meld::interpreter;
using namespace meld::parser;

namespace {

// Helper: parse and evaluate a program, return captured output
struct EvalResult {
    std::string output;
    bool success = true;
    std::string error;
};

EvalResult eval(const std::string& source, const std::string& source_file = "") {
    EvalResult result;
    
    Parser parser;
    std::vector<ast::expression> ast;
    if (!parser.parse_file(source, ast) || ast.empty()) {
        result.success = false;
        result.error = "Parse error";
        return result;
    }

    // Redirect cout to capture output
    std::ostringstream captured;
    auto* old_buf = std::cout.rdbuf(captured.rdbuf());

    try {
        AstInterpreter interp;
        if (!source_file.empty()) {
            interp.set_source_file(source_file);
        }
        interp.evaluate_program(ast);
    } catch (const InterpreterError& e) {
        result.success = false;
        result.error = e.what();
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }

    std::cout.rdbuf(old_buf);
    result.output = captured.str();
    return result;
}

std::string runfile_path(const std::string& path) {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(path);

    const char* test_srcdir = std::getenv("TEST_SRCDIR");
    const char* test_workspace = std::getenv("TEST_WORKSPACE");
    if (test_srcdir && test_workspace) {
        candidates.emplace_back(std::filesystem::path(test_srcdir) / test_workspace / path);
    }
    if (test_srcdir) {
        candidates.emplace_back(std::filesystem::path(test_srcdir) / "_main" / path);
        candidates.emplace_back(std::filesystem::path(test_srcdir) / path);
    }

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return path;
}

// ─── Integer arithmetic ─────────────────────────────────────────────

TEST(Interpreter, IntegerArithmetic) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (2 + 3)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 5\n");
}

TEST(Interpreter, IntegerSubtraction) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (10 - 3)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 7\n");
}

TEST(Interpreter, IntegerMultiplication) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (4 * 5)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 20\n");
}

TEST(Interpreter, IntegerDivision) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (15 / 3)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 5\n");
}

TEST(Interpreter, DivisionByZero) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (1 / 0)) }");
    // Division by zero throws InterpreterError caught by evaluate_program
    EXPECT_TRUE(!r.success || r.output.empty());
}

// ─── Float arithmetic ───────────────────────────────────────────────

TEST(Interpreter, FloatArithmetic) {
    auto r = eval("fnc main() -> () { println(\"r = \" + (3.14 * 2.0)) }");
    ASSERT_TRUE(r.success);
    EXPECT_NE(r.output.find("6.28"), std::string::npos);
}

// ─── String operations ──────────────────────────────────────────────

TEST(Interpreter, StringConcat) {
    auto r = eval("fnc main() -> () { println(\"Hello, \" + \"World!\") }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "Hello, World!\n");
}

TEST(Interpreter, StringIntConcat) {
    auto r = eval("fnc main() -> () { println(\"count: \" + 42) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "count: 42\n");
}

TEST(Interpreter, StringLength) {
    auto r = eval("fnc main() -> () { val s = \"hello\"\nprintln(\"len = \" + s.length) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "len = 5\n");
}

// ─── Variables ──────────────────────────────────────────────────────

TEST(Interpreter, ValDeclaration) {
    auto r = eval("fnc main() -> () { val x = 42\nprintln(\"x = \" + x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x = 42\n");
}

TEST(Interpreter, VarMutation) {
    auto r = eval("fnc main() -> () { var x = 5\nx = 10\nprintln(\"x = \" + x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x = 10\n");
}

TEST(Interpreter, ValImmutability) {
    auto r = eval("fnc main() -> () { val x = 5\nx = 10\nprintln(\"x = \" + x) }");
    // Immutability error prevents the println from executing
    EXPECT_TRUE(!r.success || r.output.empty());
}

// ─── Functions ──────────────────────────────────────────────────────

TEST(Interpreter, FunctionCallWithReturn) {
    auto r = eval("fnc add(a: int, b: int) -> int { rtn a + b }\nfnc main() -> () { println(\"r = \" + add(3, 4)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 7\n");
}

TEST(Interpreter, FunctionCallStringReturn) {
    auto r = eval("fnc greet(name: string) -> string { rtn \"Hi \" + name }\nfnc main() -> () { println(greet(\"Meld\")) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "Hi Meld\n");
}

// ─── Comparisons and booleans ───────────────────────────────────────

TEST(Interpreter, Comparisons) {
    auto r = eval("fnc main() -> () {\nval a = 5 > 3\nifTrue(a, { println(\"yes\") }) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "yes\n");
}

TEST(Interpreter, IfFalse) {
    auto r = eval("fnc main() -> () {\nval a = 1 > 10\nifFalse(a, { println(\"no\") }) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "no\n");
}

// ─── Struct field access ────────────────────────────────────────────

TEST(Interpreter, StructFieldAccess) {
    auto r = eval("fnc main() -> () {\nval p = Point { x = 10, y = 20 }\nprintln(\"x = \" + p.x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x = 10\n");
}

// ─── Module imports ─────────────────────────────────────────────────


// ─── Arrays ─────────────────────────────────────────────────────────

TEST(Interpreter, ArrayLiteral) {
    auto r = eval("fnc main() -> () { val arr = [10, 20, 30]\nprintln(arr) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "[10, 20, 30]\n");
}

TEST(Interpreter, ArrayIndexing) {
    auto r = eval("fnc main() -> () { val arr = [10, 20, 30]\nprintln(\"v = \" + arr[1]) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "v = 20\n");
}

TEST(Interpreter, MutableArrayPrimitives) {
    auto r = eval("fnc main() -> () {\nval arr = arr-new(2, 0)\narr-set-mut(arr, 1, 42)\narr-push-mut(arr, 7)\nprintln(arr-get(arr, 1))\nprintln(arr-pop-mut(arr))\nprintln(len(arr)) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "42\n7\n2\n");
}

TEST(Interpreter, MutableIntegerArrayPrimitives) {
    auto r = eval("fnc main() -> () {\nval arr = iarr-new(3, 1)\niarr-set-mut(arr, 1, 42)\nprintln(iarr-get(arr, 1))\nprintln(arr[2])\niarr-fill-mut(arr, 7)\nprintln(len(arr))\nprintln(arr[0]) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "42\n1\n3\n7\n");
}

TEST(Interpreter, HashEqualityAndBitPrimitives) {
    auto r = eval("fnc main() -> () {\nprintln(value-eq(\"a\", \"a\"))\nprintln(value-eq(\"a\", \"b\"))\nprintln(bit-and(bit-shl(1, 4), 16))\nprintln(hash-code(\"stable\") == hash-code(\"stable\")) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "true\nfalse\n16\ntrue\n");
}

TEST(Interpreter, LoopWhilePrimitiveMutatesOuterScope) {
    auto r = eval("fnc main() -> () {\nvar i = 0\nvar total = 0\nloop-while({ rtn i < 5 }, { total = total + i\ni = i + 1 })\nprintln(total) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "10\n");
}

TEST(Interpreter, StdCacheUsesPureMeldSubstrate) {
    auto source_file = runfile_path("meld-core/std/cache.meld");
    auto r = eval(R"MELD(
imp std.cache

fnc cache-test-loader(key: any) -> string {
    rtn "load-" + key
}

fnc cache-test-weigher(key: any, value: any) -> int {
    rtn value
}

fnc cache-test-pending-loader(key: any, future: any) -> any {
    rtn future
}

fnc cache-test-future-loader(key: any, future: any) -> any {
    rtn cache-future-success("future-" + key)
}

fnc cache-test-failing-loader(key: any, future: any) -> any {
    rtn cache-future-failure("fail-" + key)
}

fnc main() -> () {
    val cache = create-cache(2)
    cache-put(cache, "a", 10)
    cache-put(cache, "b", 20)
    assert(cache-get(cache, "a") == 10, "a should be present")
    cache-put(cache, "c", 30)
    assert(cache-size(cache) == 2, "cache should stay bounded")
    assert(cache-get(cache, "a") == 10, "hot key should survive admission")
    assert(cache-remove(cache, "a"), "remove should report existing key")
    assert(cache-size(cache) == 1, "remove should shrink cache")

    val expiring = create-cache-with-expiry(2, 1, 0)
    cache-put(expiring, "x", 1)
    cache-put(expiring, "y", 2)
    assert(type-of(cache-get(expiring, "x")) == "unit", "x should expire after writes")

    val cleanup-expiring = create-cache-with-expiry(4, 2, 0)
    cache-put(cleanup-expiring, "w0", 0)
    cache-put(cleanup-expiring, "w1", 1)
    cache-put(cleanup-expiring, "w2", 2)
    cache-put(cleanup-expiring, "w3", 3)
    cache-clean-up(cleanup-expiring)
    assert(cache-size(cleanup-expiring) == 3, "write expiry cleanup should remove only expired prefix")
    assert(type-of(cache-get(cleanup-expiring, "w0")) == "unit", "write expiry cleanup should remove oldest write")

    val access-expiring = create-cache-with-expiry(4, 0, 2)
    cache-put(access-expiring, "stay", 1)
    cache-put(access-expiring, "old", 2)
    assert(cache-get(access-expiring, "stay") == 1, "access expiry should refresh access order")
    cache-put(access-expiring, "n1", 3)
    cache-put(access-expiring, "n2", 4)
    cache-clean-up(access-expiring)
    assert(cache-size(access-expiring) == 3, "access expiry cleanup should preserve recently accessed entry")
    assert(type-of(cache-get(access-expiring, "old")) == "unit", "access expiry cleanup should remove oldest access")

    cache-clear(cache)
    assert(cache-size(cache) == 0, "clear should empty cache")

    val buffered = create-cache(4)
    cache-put(buffered, "b", 1)
    var buffered-reads = 0
    loop-while({ rtn buffered-reads < 64 }, {
        assert(cache-get(buffered, "b") == 1, "buffered key should remain readable")
        buffered-reads = buffered-reads + 1
    })
    assert(buffered.read-buffer-size == 0, "read buffer should drain at fixed capacity")
    assert(cache-get(buffered, "b") == 1, "buffered key should remain after drain")
    assert(buffered.read-buffer-size == 1, "read buffer should reuse fixed storage after drain")

    val churn = create-cache(8)
    val initial-index-capacity = churn.index.capacity
    var churn-index = 0
    loop-while({ rtn churn-index < 240 }, {
        cache-put(churn, "churn-" + to-string(churn-index), churn-index)
        churn-index = churn-index + 1
    })
    assert(cache-size(churn) <= 8, "churn cache should stay bounded")
    assert(churn.index.capacity == initial-index-capacity, "cache index should compact tombstones instead of growing under churn")

    val loading = create-loading-cache(2, cache-test-loader)
    assert(loading-cache-get(loading, "a") == "load-a", "loading cache should load misses")
    assert(loading-cache-get(loading, "a") == "load-a", "loading cache should retain loaded value")
    val loading-stats = cache-stats(loading)
    assert(loading-stats.load-successes == 1, "loading cache should record load success")

    val refresh-config = CacheConfig {
        max-size = 2,
        max-weight = 2,
        expire-after-write = 0,
        expire-after-access = 0,
        refresh-after-write = 1,
        loader = cache-test-loader,
        weigher = nil,
        removal-listener = nil,
        writer = nil
    }
    val refreshing = create-cache-with-config(refresh-config)
    cache-put(refreshing, "r", "old")
    cache-put(refreshing, "x", "tick")
    assert(cache-get(refreshing, "r") == "old", "refresh should serve old value first")
    assert(cache-get(refreshing, "r") == "load-r", "refresh should install loaded value")
    assert(cache-stats(refreshing).refreshes == 1, "refresh count should be recorded")

    val weighted = create-weighted-cache(3, cache-test-weigher)
    cache-put(weighted, "a", 2)
    cache-put(weighted, "b", 2)
    assert(cache-stats(weighted).eviction-weight > 0, "weighted cache should report eviction weight")

    val removals = []
    val writes = []
    val listener = { key, value, reason -> arr-push-mut(removals, reason) }
    val writer = { op, key, value, reason -> arr-push-mut(writes, op) }
    val callback-config = CacheConfig {
        max-size = 2,
        max-weight = 2,
        expire-after-write = 0,
        expire-after-access = 0,
        refresh-after-write = 0,
        loader = nil,
        weigher = nil,
        removal-listener = listener,
        writer = writer
    }
    val callback-cache = create-cache-with-config(callback-config)
    cache-put(callback-cache, "k", "v")
    cache-invalidate(callback-cache, "k")
    assert(removals[0] == "explicit", "removal listener should receive reason")
    assert(writes[0] == "write", "writer should receive writes")
    assert(writes[1] == "delete", "writer should receive deletes")

    val acache = create-async-loading-cache(2, cache-test-pending-loader)
    val f1 = async-loading-cache-get(acache, "a")
    val f2 = async-loading-cache-get(acache, "a")
    assert(type-of(f1) == "CacheFuture", "async cache should return cache futures")
    assert(cache-future-ready(f1) == false, "first async load should be pending")
    assert(cache-future-ready(f2) == false, "duplicate async load should be pending")
    assert(acache.inflight.size == 1, "duplicate async load should share inflight entry")
    async-cache-complete(acache, "a", "loaded-a")
    assert(cache-future-get(f1) == "loaded-a", "first future should complete")
    assert(cache-future-get(f2) == "loaded-a", "duplicate future should complete from same flight")
    assert(acache.inflight.size == 0, "completed async load should clear inflight entry")
    assert(cache-get(acache.cache, "a") == "loaded-a", "async completion should populate backing cache")
    val cached-future = async-loading-cache-get(acache, "a")
    assert(cache-future-ready(cached-future), "async cache hit should be ready")
    assert(cache-future-get(cached-future) == "loaded-a", "async cache hit should return cached value")
    assert(cache-stats(acache.cache).load-successes == 1, "async cache should record one shared load")

    val immediate-cache = create-async-loading-cache(2, cache-test-future-loader)
    val immediate = async-loading-cache-get(immediate-cache, "b")
    assert(cache-future-get(immediate) == "future-b", "async loader future should be chained")
    assert(cache-get(immediate-cache.cache, "b") == "future-b", "chained async load should populate cache")

    val failed-cache = create-async-loading-cache(2, cache-test-failing-loader)
    val failed = async-loading-cache-get(failed-cache, "bad")
    assert(cache-future-ready(failed), "failed async load should be ready")
    assert(cache-future-successful(failed) == false, "failed async load should not be successful")
    assert(cache-future-error(failed) == "fail-bad", "failed async load should expose error")
    assert(failed-cache.inflight.size == 0, "failed async load should clear inflight entry")
    assert(cache-stats(failed-cache.cache).load-failures == 1, "async cache should record load failures")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncRingQueueWrapAndSteal) {
    auto source_file = runfile_path("meld-core/std/async/queue.meld");
    auto r = eval(R"MELD(
imp std.async.queue

fnc main() -> () {
    val q = create-ring-queue(3)
    assert(ring-empty(q), "queue should start empty")
    assert(ring-push(q, 1), "push 1")
    assert(ring-push(q, 2), "push 2")
    assert(ring-push(q, 3), "push 3")
    assert(ring-full(q), "queue should be full")
    assert(ring-push(q, 4) == false, "full queue should reject push")

    val first = ring-pop(q)
    assert(first.found, "first pop should find value")
    assert(first.value == 1, "first pop should preserve FIFO order")
    assert(ring-push(q, 4), "push should wrap after pop")
    assert(ring-pop(q).value == 2, "second pop should preserve FIFO order")
    assert(ring-pop(q).value == 3, "third pop should preserve FIFO order")
    assert(ring-pop(q).value == 4, "wrapped value should pop last")
    assert(ring-empty(q), "queue should be empty after pops")

    val source = create-ring-queue(8)
    val target = create-ring-queue(8)
    ring-push(source, 10)
    ring-push(source, 11)
    ring-push(source, 12)
    ring-push(source, 13)
    assert(ring-steal-half(source, target) == 2, "steal should move half")
    assert(ring-pop(target).value == 10, "stolen queue should keep FIFO order")
    assert(ring-pop(target).value == 11, "stolen queue should keep FIFO order")
    assert(ring-pop(source).value == 12, "source should retain remaining half")
    assert(ring-pop(source).value == 13, "source should retain remaining half")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncAtomicPrimitivesAndQueue) {
    auto source_file = runfile_path("meld-core/std/async/atomic_queue.meld");
    auto r = eval(R"MELD(
imp std.async.atomic
imp std.async.atomic_queue

fnc main() -> () {
    val cell = atomic-int(5)
    assert(atomic-load(cell) == 5, "atomic load should read initial value")
    atomic-store(cell, 7)
    assert(atomic-load(cell) == 7, "atomic store should publish value")
    assert(atomic-exchange(cell, 9) == 7, "atomic exchange should return old value")
    assert(atomic-fetch-add(cell, 3) == 9, "fetch-add should return previous value")
    assert(atomic-load(cell) == 12, "fetch-add should update value")

    val failed = atomic-compare-exchange(cell, 9, 1)
    assert(failed.success == false, "CAS should fail when expected mismatches")
    assert(failed.value == 12, "failed CAS should report observed value")
    val succeeded = atomic-compare-exchange(cell, 12, 1)
    assert(succeeded.success, "CAS should succeed when expected matches")
    assert(atomic-load(cell) == 1, "successful CAS should store desired value")
    atomic-destroy(cell)

    val queue = create-atomic-queue(3)
    assert(atomic-queue-pop(queue).found == false, "new queue should be empty")
    assert(atomic-queue-push(queue, "a"), "first push should fit")
    assert(atomic-queue-push(queue, "b"), "second push should fit")
    assert(atomic-queue-push(queue, "c"), "third push should fit")
    assert(atomic-queue-push(queue, "d") == false, "full queue should reject push")
    assert(atomic-queue-pop(queue).value == "a", "queue should pop FIFO")
    assert(atomic-queue-push(queue, "d"), "queue should accept after pop")
    assert(atomic-queue-pop(queue).value == "b", "queue should preserve FIFO")
    assert(atomic-queue-pop(queue).value == "c", "queue should preserve FIFO")
    assert(atomic-queue-pop(queue).value == "d", "queue should preserve wraparound FIFO")
    assert(atomic-queue-pop(queue).found == false, "drained queue should be empty")
    atomic-queue-destroy(queue)

    assert(worker-cpu-count() >= 1, "cpu count should be at least one")
    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdCacheBenchmarkWorkloadsAreDeterministic) {
    auto source_file = runfile_path("meld-core/std/test/cache/benchmark.meld");
    auto r = eval(R"MELD(
imp std.test.cache.benchmark

fnc main() -> () {
    val uniform = cache-benchmark-uniform(32, 256, 256)
    assert(uniform.operations == 256, "uniform workload should report operation count")
    assert(uniform.hits + uniform.misses == 256, "uniform workload should account for requests")
    assert(uniform.size <= 32, "uniform workload should stay bounded")

    val hot = cache-benchmark-hotset(32, 16, 256, 256)
    assert(hot.operations == 256, "hotset workload should report operation count")
    assert(hot.hit-rate > uniform.hit-rate, "hotset workload should outperform uniform workload")
    assert(hot.size <= 32, "hotset workload should stay bounded")

    val scan = cache-benchmark-scan-resistant(16, 4, 32, 4)
    assert(scan.operations == 144, "scan workload should report operation count")
    assert(scan.size <= 16, "scan workload should stay bounded")
    assert(scan.evictions > 0, "scan workload should exercise eviction")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncPollableSchedulerIntegration) {
    auto source_file = runfile_path("meld-core/std/async/scheduler.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler

fnc main() -> () {
    assert(block-on(ready(7)) == 7, "ready/block-on should complete")
    val chained = then(ready(20), fnc(value: int) -> any {
        rtn ready(value + 22)
    })
    assert(block-on(chained) == 42, "then/block-on should complete")

    var polls = 0
    val self-waking = Pollable {
        poll-fn = fnc(ctx: PollContext) -> any {
            polls = polls + 1
            rtn when(polls == 1).then({
                wake-task(ctx.waker)
                wake-task(ctx.waker)
                rtn Pending()
            }).else({
                rtn Ready(polls)
            })
        }
    }

    val runtime = create-runtime-with-workers(2)
    val task = spawn-on-worker(runtime, 1, self-waking)
    assert(tick-worker(runtime, 1), "first poll should run")
    assert(task.state == "pending", "task should park after first poll")
    assert(tick-worker(runtime, 0), "peer worker should steal woken task")
    assert(task.state == "completed", "task should complete after wake")
    assert(task.worker-id == 0, "stolen task should update owner")
    assert(task.result == 2, "duplicate wakes must not duplicate polls")
    assert(tick-worker(runtime, 1) == false, "stale duplicate wake must not run")
    assert(polls == 2, "task should be polled exactly twice")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncWorkerDriverLeasesAndDrives) {
    auto source_file = runfile_path("meld-core/std/async/driver.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.driver

fnc main() -> () {
    val runtime = create-runtime-with-workers(2)
    val driver = create-worker-driver(runtime)
    assert(driver-worker-count(driver) == 2, "driver should expose one lease per worker")
    assert(worker-lease-owner(driver, 0) == 0, "new lease should be free")
    assert(try-acquire-worker(driver, 0, 101), "first token should acquire lease")
    assert(worker-lease-owner(driver, 0) == 101, "lease should publish owner token")
    assert(try-acquire-worker(driver, 0, 202) == false, "second token must not steal live lease")
    assert(release-worker(driver, 0, 202) == false, "wrong token must not release lease")
    assert(worker-lease-owner(driver, 0) == 101, "wrong release should preserve owner")
    assert(release-worker(driver, 0, 101), "matching token should release lease")
    assert(worker-lease-owner(driver, 0) == 0, "released lease should be free")
    assert(try-acquire-worker(driver, 0, 0) == false, "zero token is reserved for free leases")
    assert(try-acquire-worker(driver, -1, 101) == false, "negative worker id should be rejected")
    assert(worker-lease-owner(driver, 7) == -1, "invalid worker should report sentinel owner")

    var polls = 0
    val self-waking = Pollable {
        poll-fn = fnc(ctx: PollContext) -> any {
            polls = polls + 1
            rtn when(polls == 1).then({
                wake-task(ctx.waker)
                rtn Pending()
            }).else({
                rtn Ready(polls)
            })
        }
    }

    val task = spawn-on-worker(runtime, 1, self-waking)
    assert(drive-worker-once(driver, 1, 101, 0), "driver should run leased worker")
    assert(task.state == "pending", "first driver poll should park task")
    assert(worker-lease-owner(driver, 1) == 0, "driver should release lease after tick")
    assert(drive-worker-once(driver, 0, 202, 0), "peer driver should steal woken task")
    assert(task.state == "completed", "stolen task should complete")
    assert(task.worker-id == 0, "stolen task should move to driving worker")
    assert(task.result == 2, "task should complete on second poll")
    assert(polls == 2, "driver should not duplicate polls")
    destroy-worker-driver(driver)

    val round = create-runtime-with-workers(3)
    val round-driver = create-worker-driver(round)
    val a = spawn-on-worker(round, 0, ready(1))
    val b = spawn-on-worker(round, 1, ready(2))
    val c = spawn-on-worker(round, 2, ready(3))
    val active = drive-runtime-steps(round-driver, 303, 6, 0)
    assert(active >= 3, "round-robin driver should visit active workers")
    assert(a.state == "completed" && b.state == "completed" && c.state == "completed", "round-robin driver should complete ready tasks")
    assert(a.result == 1 && b.result == 2 && c.result == 3, "round-robin driver should preserve task results")
    destroy-worker-driver(round-driver)

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncTaskArenaReleasesAndRejectsOverflow) {
    auto source_file = runfile_path("meld-core/std/async/scheduler.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler

fnc main() -> () {
    val runtime = create-runtime-with-capacity(1, 2)
    assert(runtime-task-free-slots(runtime) == 2, "arena should start empty")

    val first = spawn-pollable(runtime, ready(11))
    assert(first.slot >= 0, "task should allocate a slot")
    assert(runtime-live-tasks(runtime) == 1, "spawn should consume one slot")
    assert(tick-runtime(runtime), "ready task should run")
    assert(first.state == "completed", "ready task should complete")
    assert(first.result == 11, "ready task should store result")
    assert(first.released, "completed task should release slot")
    assert(runtime-task-free-slots(runtime) == 2, "completion should return slot")
    assert(is-nil(find-task(runtime, first.id)), "completed task should leave wake index")

    var polls = 0
    val self-waking = Pollable {
        poll-fn = fnc(ctx: PollContext) -> any {
            polls = polls + 1
            wake-task(ctx.waker)
            rtn Pending()
        }
    }

    val parked = spawn-pollable(runtime, self-waking)
    assert(tick-runtime(runtime), "parked task should poll once")
    assert(parked.state == "pending", "parked task should be pending")
    assert(runtime-live-tasks(runtime) == 1, "pending task should hold slot")
    cancel-frame-in-runtime(runtime, parked)
    assert(parked.state == "completed", "cancelled task should complete")
    assert(parked.error == "cancelled", "cancelled task should record error")
    assert(runtime-task-free-slots(runtime) == 2, "cancel should release slot")
    assert(tick-runtime(runtime) == false, "stale wake should not reschedule cancelled task")
    assert(polls == 1, "cancelled stale wake should not poll again")

    var cleaned = 0
    val cleanup-work = Pollable {
        poll-fn = fnc(ctx: PollContext) -> any {
            rtn Pending()
        },
        cleanup-fn = fnc() -> () {
            cleaned = cleaned + 1
        }
    }
    val cleanup-task = spawn-pollable(runtime, cleanup-work)
    cancel-frame-in-runtime(runtime, cleanup-task)
    cancel-frame-in-runtime(runtime, cleanup-task)
    assert(cleaned == 1, "cleanup should run once on cancellation")

    val saturated = create-runtime-with-capacity(1, 2)
    val a = spawn-pollable(saturated, pending())
    val b = spawn-pollable(saturated, pending())
    val c = spawn-pollable(saturated, pending())
    assert(a.slot >= 0 && b.slot >= 0, "first two tasks should allocate")
    assert(c.slot == -1, "overflow task should not allocate")
    assert(c.state == "completed", "overflow task should be completed")
    assert(c.error == "task arena full", "overflow task should expose arena error")
    assert(runtime-live-tasks(saturated) == 2, "overflow should not consume a slot")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncAwaitTaskWakesWaiters) {
    auto source_file = runfile_path("meld-core/std/async/task.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.task

fnc main() -> () {
    val runtime = create-runtime-with-workers(2)
    val child-frame = spawn-on-worker(runtime, 1, ready(41))
    val child = Task { runtime = runtime, frame = child-frame }
    val parent-work = then(await-task(child), fnc(value: int) -> any {
        rtn ready(value + 1)
    })
    val parent-frame = spawn-on-worker(runtime, 0, parent-work)

    assert(tick-worker(runtime, 0), "parent should poll before child completes")
    assert(parent-frame.state == "pending", "parent should park while child is pending")
    assert(task-waiter-count(child-frame) == 1, "await should register parent waker")
    assert(tick-worker(runtime, 1), "child worker should complete child")
    assert(child-frame.state == "completed", "child should complete")
    assert(task-waiter-count(child-frame) == 0, "child completion should drain waiters")
    assert(tick-worker(runtime, 0), "child completion should wake parent")
    assert(parent-frame.state == "completed", "parent should complete after child wake")
    assert(parent-frame.result == 42, "await should forward child result")

    val cancelling = create-runtime-with-workers(2)
    val stalled-frame = spawn-on-worker(cancelling, 1, pending())
    val stalled = Task { runtime = cancelling, frame = stalled-frame }
    val waiter-frame = spawn-on-worker(cancelling, 0, await-task(stalled))

    assert(tick-worker(cancelling, 0), "waiter should poll before cancellation")
    assert(waiter-frame.state == "pending", "waiter should park on pending child")
    assert(task-waiter-count(stalled-frame) == 1, "pending child should retain waiter")
    cancel-task(stalled)
    assert(stalled-frame.state == "completed", "cancelled child should complete")
    assert(task-waiter-count(stalled-frame) == 0, "cancel should drain waiters")
    assert(tick-worker(cancelling, 0), "cancel should wake waiter")
    assert(waiter-frame.state == "completed", "waiter should finish after cancellation")
    assert(waiter-frame.error == "cancelled", "await should forward child cancellation")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncBlockOnIteratesRepeatedWakes) {
    auto source_file = runfile_path("meld-core/std/async/scheduler.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler

fnc main() -> () {
    var polls = 0
    val repeated = Pollable {
        poll-fn = fnc(ctx: PollContext) -> any {
            polls = polls + 1
            rtn when(polls < 64).then({
                wake-task(ctx.waker)
                rtn Pending()
            }).else({
                rtn Ready(polls)
            })
        }
    }

    assert(block-on(repeated) == 64, "block-on should drive repeated wakes")
    assert(polls == 64, "block-on should not duplicate polls")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncChannelReceiveWakesTask) {
    auto source_file = runfile_path("meld-core/std/async/channel.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.channel

fnc main() -> () {
    val runtime = create-runtime()
    val ch = create-channel()
    val task = spawn-pollable(runtime, channel-recv(ch))

    assert(tick-runtime(runtime), "receiver should be polled once")
    assert(task.state == "pending", "empty channel should park receiver")
    assert(channel-send(ch, 42), "send should succeed")
    assert(tick-runtime(runtime), "send should wake receiver")
    assert(task.state == "completed", "receiver should complete")
    assert(task.result == 42, "receiver should get sent value")
    assert(tick-runtime(runtime) == false, "runtime should be idle after receive")

    val bounded = create-channel-with-capacity(2)
    assert(channel-send(bounded, 1), "bounded channel first send should fit")
    assert(channel-send(bounded, 2), "bounded channel second send should fit")
    assert(channel-send(bounded, 3) == false, "bounded channel should reject full send")
    assert(try-recv(bounded) == 1, "bounded channel should preserve FIFO order")
    assert(channel-send(bounded, 3), "bounded channel should accept after receive")
    assert(try-recv(bounded) == 2, "bounded channel should preserve FIFO order after wrap")
    assert(try-recv(bounded) == 3, "bounded channel should preserve wrapped value")
    assert(is-empty(bounded), "bounded channel should be empty")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncReactorPipeRoundTrip) {
    auto source_file = runfile_path("meld-core/std/async/reactor.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.reactor

fnc drive(runtime: Runtime, remaining: int) -> () {
    when(remaining > 0).then({
        tick-runtime(runtime)
        drive(runtime, remaining - 1)
    })
}

fnc main() -> () {
    val runtime = create-runtime()
    val fds = kernel.fd-pipe()
    val reader = spawn-pollable(runtime, read-bytes(runtime.reactor, fds[0], 16))
    val writer = spawn-pollable(runtime, write-bytes(runtime.reactor, fds[1], "hi"))

    drive(runtime, 8)

    assert(writer.state == "completed", "writer should complete")
    assert(writer.result == 2, "writer should report bytes written")
    assert(reader.state == "completed", "reader should complete")
    assert(reader.result == "hi", "reader should receive pipe payload")

    close-fd(runtime.reactor, fds[0])
    close-fd(runtime.reactor, fds[1])
    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncTimerSleepCompletesThroughReactor) {
    auto source_file = runfile_path("meld-core/std/async/timer.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.timer

fnc main() -> () {
    val runtime = create-runtime()
    val task = spawn-pollable(runtime, sleep(runtime.reactor, 1))

    assert(tick-runtime(runtime), "sleep should arm and park on first poll")
    assert(task.state == "pending", "sleep should be pending before timer fires")
    assert(tick-runtime-timeout(runtime, 50), "timer should wake through reactor")
    assert(task.state == "completed", "sleep should complete after timer readiness")

    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, StdAsyncTcpLoopbackRoundTrip) {
    auto source_file = runfile_path("meld-core/std/async/net.meld");
    auto r = eval(R"MELD(
imp std.async.core
imp std.async.scheduler
imp std.async.net

fnc drive(runtime: Runtime, remaining: int) -> () {
    when(remaining > 0).then({
        tick-runtime-timeout(runtime, 1)
        drive(runtime, remaining - 1)
    })
}

fnc main() -> () {
    val runtime = create-runtime()
    val listener = listen("127.0.0.1", 0)
    val port = local-port(listener)

    val accepted = spawn-pollable(runtime, accept(runtime.reactor, listener))
    val connected = spawn-pollable(runtime, connect(runtime.reactor, "127.0.0.1", port))
    drive(runtime, 80)

    assert(accepted.state == "completed", "accept should complete")
    assert(connected.state == "completed", "connect should complete")

    val server = accepted.result
    val client = connected.result
    val server-read = spawn-pollable(runtime, read-stream(runtime.reactor, server, 16))
    val client-write = spawn-pollable(runtime, write-stream(runtime.reactor, client, "ping"))
    drive(runtime, 80)

    assert(client-write.result == 4, "client should write all bytes")
    assert(server-read.result == "ping", "server should receive client payload")

    val client-read = spawn-pollable(runtime, read-stream(runtime.reactor, client, 16))
    val server-write = spawn-pollable(runtime, write-stream(runtime.reactor, server, "pong"))
    drive(runtime, 80)

    assert(server-write.result == 4, "server should write all bytes")
    assert(client-read.result == "pong", "client should receive server payload")

    close-stream(runtime.reactor, client)
    close-stream(runtime.reactor, server)
    close-listener(runtime.reactor, listener)
    println("ok")
}
)MELD", source_file);
    ASSERT_TRUE(r.success) << r.error << "\noutput:\n" << r.output;
    EXPECT_EQ(r.output, "ok\n");
}

TEST(Interpreter, KernelWakeQueuePrimitives) {
    auto r = eval("fnc main() -> () {\nkernel.wake-task(7, 42, 3)\nval wakes = kernel.take-wakes(7)\nprintln(len(wakes))\nprintln(wakes[0].task-id)\nprintln(wakes[0].generation)\nprintln(len(kernel.take-wakes(7))) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "1\n42\n3\n0\n");
}

TEST(Interpreter, KernelSecureZeroPrimitive) {
    auto r = eval("fnc main() -> () {\nval secret = \"secret\"\nkernel.secure-zero(secret)\nprintln(len(secret)) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "0\n");
}

TEST(Interpreter, KernelReactorCreatePrimitive) {
    auto r = eval("fnc main() -> () {\nval reactor = kernel.reactor-create(\"epoll\")\nprintln(reactor > 0) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "true\n");
}

TEST(Interpreter, KernelReactorPipeReadiness) {
    auto r = eval("fnc main() -> () {\nval fds = kernel.fd-pipe()\nval reactor = kernel.reactor-create(\"epoll\")\nkernel.reactor-register(reactor, fds[0], 1, 3, 11, 0)\nprintln(len(kernel.reactor-poll(reactor, 0)))\nkernel.fd-write(fds[1], \"x\", 0)\nval events = kernel.reactor-poll(reactor, 50)\nprintln(len(events) > 0)\nval ready = kernel.reactor-ready(reactor, fds[0], 1)\nprintln(ready.fd == fds[0])\nprintln(kernel.fd-read(fds[0], 8))\nkernel.fd-close(fds[0])\nkernel.fd-close(fds[1]) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "0\ntrue\ntrue\nx\n");
}

TEST(Interpreter, KernelTimerReactorReadiness) {
    auto r = eval("fnc main() -> () {\nval reactor = kernel.reactor-create(\"epoll\")\nval timer = kernel.timer-create()\nkernel.timer-arm(timer, 1)\nkernel.reactor-register(reactor, timer, 1, 5, 99, 0)\nval events = kernel.reactor-poll(reactor, 50)\nprintln(len(events) > 0)\nval ready = kernel.reactor-ready(reactor, timer, 1)\nprintln(ready.fd == timer)\nkernel.timer-cancel(timer) }");
    ASSERT_TRUE(r.success) << r.error;
    EXPECT_EQ(r.output, "true\ntrue\n");
}

// ─── Template strings ───────────────────────────────────────────────

TEST(Interpreter, TemplateString) {
    auto r = eval("fnc main() -> () { val x = 42\nprintln(`x is ${x}`) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x is 42\n");
}

// ─── Recursion ──────────────────────────────────────────────────────

TEST(Interpreter, Recursion) {
    auto r = eval("fnc countdown(n: int) -> () {\nprintln(n)\nifTrue(n > 0, { countdown(n - 1) }) }\nfnc main() -> () { countdown(3) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "3\n2\n1\n0\n");
}

// ─── Match ──────────────────────────────────────────────────────────

TEST(Interpreter, Match) {
    auto r = eval("fnc main() -> () { val x = match(2, [[1, { rtn \"one\" }], [2, { rtn \"two\" }]])\nprintln(x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "two\n");
}

// ─── When/Then/Else ─────────────────────────────────────────────────

TEST(Interpreter, WhenThenElse) {
    auto r = eval("fnc main() -> () { val x = when(true).then({ rtn \"yes\" }).else({ rtn \"no\" })\nprintln(x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "yes\n");
}

TEST(Interpreter, WhenThenElseFalse) {
    auto r = eval("fnc main() -> () { val x = when(false).then({ rtn \"yes\" }).else({ rtn \"no\" })\nprintln(x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "no\n");
}

TEST(Interpreter, Factorial) {
    auto r = eval("fnc factorial(n: int) -> int { rtn when(n == 0).then({ rtn 1 }).else({ rtn n * factorial(n - 1) }) }\nfnc main() -> () { println(\"r = \" + factorial(5)) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "r = 120\n");
}

// ─── Method dispatch ────────────────────────────────────────────────

TEST(Interpreter, MethodDispatch) {
    auto r = eval("fnc sum(p: Point) -> int { rtn p.x + p.y }\nfnc main() -> () { val p = Point { x = 3, y = 4 }\nprintln(\"s = \" + p.sum()) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "s = 7\n");
}

// ─── Field mutation ─────────────────────────────────────────────────

TEST(Interpreter, FieldMutation) {
    auto r = eval("fnc main() -> () { var p = Point { x = 1, y = 2 }\np.x = 99\nprintln(\"x = \" + p.x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x = 99\n");
}

// ─── Float arithmetic ───────────────────────────────────────────────

TEST(Interpreter, FloatArithmeticPrecise) {
    auto r = eval("fnc main() -> () { val x = 3.14 * 2.0\nprintln(\"x = \" + x) }");
    ASSERT_TRUE(r.success);
    EXPECT_NE(r.output.find("6.28"), std::string::npos);
}

TEST(Interpreter, NilValue) {
    auto r = eval("fnc main() -> () { val x = nil\nprintln(\"x = \" + x) }");
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.output, "x = empty\n");
}

} // namespace
