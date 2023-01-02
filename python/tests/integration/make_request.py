import base64
from typing import Any, List

import msgpack  # type: ignore

from cradle.session import Session


# Creates a Python data structure which, when converted to JSON, is the JSON serialization of
# a C++ function_request_impl object
def make_function_request_impl(session: Session, uuid_base_ext: str, title: str, id_offset: int, args: List) -> Any:
    # Request metadata
    uuid = f'{uuid_base_ext}+{session.git_version}'
    my_id = 2147483649 + id_offset

    # Args
    all_args = [session.context_id] + args
    args_data = {f'tuple_element{i}': arg for i, arg in enumerate(all_args)}

    return \
        {'impl': {'polymorphic_id': my_id,
                  'polymorphic_name': uuid,
                  'ptr_wrapper': {'data': {'args': args_data,
                                           'uuid': {'value0': uuid}},
                                  'id': my_id}},
         'title': title}


def make_post_iss_object_request(session: Session, value: str, id_offset: int = 0) -> Any:
    uuid_base = 'rq_post_iss_object'
    uuid_ext = 'blob'
    title = 'post_iss_object'
    msgpack_encoded = msgpack.packb(value, use_bin_type=True)
    base64_encoded = base64.standard_b64encode(msgpack_encoded)
    ascii_encoded = str(base64_encoded, encoding='ascii')
    args = [
        'array/string',
        {'size': len(msgpack_encoded), 'blob': ascii_encoded}
    ]

    return make_function_request_impl(session, f'{uuid_base}-{uuid_ext}', title, id_offset, args)


def make_get_iss_object_metadata_request(session: Session, object_id: Any, id_offset: int = 0) -> Any:
    uuid_base = 'rq_get_iss_object_metadata'
    uuid_ext = 'plain' if isinstance(object_id, str) else 'subreq-full'
    title = 'get_iss_object_metadata'
    args = [object_id]

    return make_function_request_impl(session, f'{uuid_base}-{uuid_ext}', title, id_offset, args)


def make_retrieve_immutable_object_request(session: Session, immutable_id: Any, id_offset: int = 0) -> Any:
    uuid_base = 'rq_retrieve_immutable_object'
    uuid_ext = 'plain' if isinstance(immutable_id, str) else 'subreq-full'
    title = 'retrieve_immutable_object'
    args = [immutable_id]

    return make_function_request_impl(session, f'{uuid_base}-{uuid_ext}', title, id_offset, args)


def make_resolve_iss_object_to_immutable_request(session: Session, object_id: Any, id_offset: int = 0) -> Any:
    uuid_base = 'rq_resolve_iss_object_to_immutable'
    uuid_ext = 'plain' if isinstance(object_id, str) else 'subreq-full'
    title = 'resolve_iss_object_to_immutable'
    ignore_upgrades = False
    args = [object_id, ignore_upgrades]

    return make_function_request_impl(session, f'{uuid_base}-{uuid_ext}', title, id_offset, args)
