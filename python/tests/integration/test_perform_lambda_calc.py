import random

import pytest

from decode_json import get_tasklets_in_pool


# The "sleep" sample lambda takes one floating-point argument,
# sleeps for so many seconds, then returns 42.
def create_calculation(seconds):
    return \
        {
            "lambda": {
                "function": "sleep",
                "args": [
                    {
                        "value": seconds
                    }
                ]
            }
        }


def num_tasklets_in_lambda_pool(session):
    pool_name = 'lambda@any'
    json = session.query_introspection_status(True)
    tasklets_in_pool = get_tasklets_in_pool(json, pool_name)
    return len(tasklets_in_pool)


def test_perform_local_calc(session):
    session.introspection_set_enabled(True)
    session.introspection_clear_admin()

    t0 = 0.2 + random.random() / 10.0
    calc0 = create_calculation(t0)

    # Two parallel requests should lead to exactly one lambda call; the second
    # result should come from the cache.
    print('perform_local_calc(first lambda)')
    new_calculation_result = session.perform_local_calc(calc0, num_requests=2)
    assert new_calculation_result == pytest.approx(42)
    assert num_tasklets_in_lambda_pool(session) == 1

    # Two more parallel requests with the same input; both results should come
    # from the cache.
    print('perform_local_calc(first lambda again)')
    cached_calculation_result = session.perform_local_calc(calc0, num_requests=2)
    assert cached_calculation_result == new_calculation_result
    assert num_tasklets_in_lambda_pool(session) == 1

    # Two more parallel requests with different input should trigger exactly one
    # lambda call.
    calc1 = create_calculation(t0 + 0.1)
    print('perform_local_calc(second lambda)')
    another_calculation_result = session.perform_local_calc(calc1, num_requests=2)
    assert another_calculation_result == pytest.approx(42)
    assert num_tasklets_in_lambda_pool(session) == 2
