import base64
from typing import Any, List

import msgpack  # type: ignore

from cradle.session import Session


# Creates a Python data structure which, when converted to JSON,
# becomes the JSON serialization of a C++ function_request_erased object
def make_req(session: Session, uuid_base: str, title: str, args: List) -> Any:
    # Request metadata
    uuid = uuid_base

    # Args
    args_data = {f'tuple_element{i}': arg for i, arg in enumerate(args)}

    return \
        {'uuid': uuid,
         'plain_uuid': '',
         'title': title,
         'base_millis': 100,
         'max_attempts': 3,
         'args': args_data}


# Creates the representation of a function_request_erased object for a root request.
def make_root_req(session: Session, uuid_base: str, title: str, args: List) -> Any:
    return make_req(session, uuid_base, title, [session.context_id] + args)


# Converts an argument to the representation of a function_request_erased object returning that value;
# similar to its same-named C++ counterpart.
# If the argument already represents a function_request_erased object, it is returned as-is.
# type_id: 'string' or 'blob'
# The function is assumed to be a coroutine.
def normalized_arg(session: Session, type_id: str, arg: Any) -> Any:
    try:
        # An encoded request is a Dict having 'uuid', 'title' and 'args' entries.
        arg['uuid']
        arg['title']
        arg['args']
        return arg
    except (KeyError, TypeError):
        pass
    return make_req(session, f'normalization<{type_id},coro>', 'arg', [arg])


def make_post_iss_object_request(session: Session, value: str) -> Any:
    uuid_base = 'rq_post_iss_object+full'
    title = 'post_iss_object'
    msgpack_encoded = msgpack.packb(value, use_bin_type=True)
    base64_encoded = base64.standard_b64encode(msgpack_encoded)
    ascii_encoded = str(base64_encoded, encoding='ascii')
    inner_args = \
        {'as_file': False,
         'size': len(msgpack_encoded),
         'blob': ascii_encoded}
    args = ['array/string', normalized_arg(session, 'blob', inner_args)]

    return make_root_req(session, uuid_base, title, args)


def make_get_iss_object_metadata_request(session: Session, object_id: Any) -> Any:
    uuid_base = 'rq_get_iss_object_metadata+full'
    title = 'get_iss_object_metadata'
    args = [normalized_arg(session, 'string', object_id)]

    return make_root_req(session, uuid_base, title, args)


def make_retrieve_immutable_object_request(session: Session, immutable_id: Any) -> Any:
    uuid_base = 'rq_retrieve_immutable_object+full'
    title = 'retrieve_immutable_object'
    args = [normalized_arg(session, 'string', immutable_id)]

    return make_root_req(session, uuid_base, title, args)


def make_resolve_iss_object_to_immutable_request(session: Session, object_id: Any) -> Any:
    uuid_base = 'rq_resolve_iss_object_to_immutable+full'
    title = 'resolve_iss_object_to_immutable'
    ignore_upgrades = False
    args = [normalized_arg(session, 'string', object_id), ignore_upgrades]

    return make_root_req(session, uuid_base, title, args)
