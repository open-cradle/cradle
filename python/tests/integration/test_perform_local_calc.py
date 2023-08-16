import random

import pytest

from decode_json import get_tasklets_in_pool


# There are two function calls in here.
def create_calculation(a, b, c):
    return \
        {
            "let": {
                "variables": {
                    "x": {
                        "function": {
                            "account": "mgh",
                            "app": "dosimetry",
                            "name": "addition",
                            "args": [
                                {
                                    "value": a
                                },
                                {
                                    "value": b
                                }
                            ]
                        }
                    }
                },
                "in": {
                    "function": {
                        "account": "mgh",
                        "app": "dosimetry",
                        "name": "addition",
                        "args": [
                            {
                                "variable": "x",
                            },
                            {
                                "value": c
                            }
                        ]
                    }
                }
            }
        }


def num_tasklets_in_docker_pool(session):
    pool_name = 'local@dosimetry'
    json = session.query_introspection_status(True)
    tasklets_in_pool = get_tasklets_in_pool(json, pool_name)
    return len(tasklets_in_pool)


def test_perform_local_calc(session):
    session.introspection_set_enabled(True)
    session.introspection_clear_admin()

    a = random.random()
    b = random.random()
    c = random.random()

    calculation = create_calculation(a, b, c)

    # For the first request, the two function calls should be evaluated
    # by the Docker container.
    print('perform_local_calc')
    new_calculation_result = session.perform_local_calc(calculation)
    assert new_calculation_result == pytest.approx(a + b + c)
    assert num_tasklets_in_docker_pool(session) == 2

    # Second request with same inputs; the result should now come
    # from the cache.
    print('perform_local_calc again')
    cached_calculation_result = session.perform_local_calc(calculation)
    assert cached_calculation_result == new_calculation_result
    assert num_tasklets_in_docker_pool(session) == 2
