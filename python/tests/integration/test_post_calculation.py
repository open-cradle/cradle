import random

import pytest

from decode_json import get_tasklets_in_pool


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


def num_tasklets_in_http_pool(session):
    pool_name = 'HTTP'
    json = session.query_introspection_status(True)
    tasklets_in_pool = get_tasklets_in_pool(json, pool_name)
    return len(tasklets_in_pool)


def test_post_calculation(session):
    session.introspection_set_enabled(True)
    session.introspection_clear_admin()

    a = random.random()
    b = random.random()
    c = random.random()

    calculation = create_calculation(a, b, c)

    # The first post_calculation request should be handled by Thinknode.
    print('first post_calculation')
    new_calculation_id = session.post_calculation(calculation)
    num_http_requests_after_first_post = num_tasklets_in_http_pool(session)

    # First request retrieving the ISS object for the calculation id.
    # The result should come from Thinknode.
    print('first get_iss_object')
    first_get_result = session.get_iss_object(new_calculation_id)
    assert first_get_result == pytest.approx(a + b + c)
    num_http_requests_after_first_get = num_tasklets_in_http_pool(session)
    assert num_http_requests_after_first_get > num_http_requests_after_first_post

    # Second request retrieving the ISS object for the calculation id.
    # The result should come from the cache.
    print('second get_iss_object')
    second_get_result = session.get_iss_object(new_calculation_id)
    assert second_get_result == first_get_result
    num_http_requests_after_second_get = num_tasklets_in_http_pool(session)
    assert num_http_requests_after_second_get == num_http_requests_after_first_get

    # A second post_calculation request with same inputs as before.
    # The result should come from the cache.
    print('second post_calculation')
    cached_calculation_id = session.post_calculation(calculation)
    assert cached_calculation_id == new_calculation_id
    num_http_requests_after_second_post = num_tasklets_in_http_pool(session)
    assert num_http_requests_after_second_post == num_http_requests_after_second_get
