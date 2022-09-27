import json
from typing import Any, Dict, Optional

import msgpack  # type: ignore


Content = Dict
Object = Any


class Command:
    def __init__(self, context_id: str):
        self.context_id = context_id

    def name(self) -> str:
        raise NotImplementedError

    def create_content(self) -> Content:
        raise NotImplementedError

    def decode_content(self, _content: Content) -> Optional[Object]:
        raise NotImplementedError


class RegistrationCommand(Command):
    def __init__(self, context_id: str, api_url: str, api_token: str):
        super().__init__(context_id)
        self.api_url = api_url
        self.api_token = api_token

    def name(self) -> str:
        return 'registration'

    def create_content(self) -> Content:
        return \
            {
                'registration': {
                    'name': 'thinknode.python.lib',
                    'session': {
                        'api_url': self.api_url,
                        'access_token': self.api_token,
                    }
                }
            }

    def decode_content(self, _content: Content) -> None:
        return None


class IssObjectCommand(Command):
    def __init__(self, context_id: str, object_id: str, ignore_upgrades: bool = False):
        super().__init__(context_id)
        self.object_id = object_id
        self.ignore_upgrades = ignore_upgrades

    def name(self) -> str:
        return 'iss_object'

    def create_content(self) -> Content:
        return \
            {
                'iss_object': {
                    'context_id': self.context_id,
                    'object_id': self.object_id,
                    'encoding': 'msgpack',
                    'ignore_upgrades': self.ignore_upgrades
                }
            }

    def decode_content(self, content: Content) -> Object:
        gio = content['iss_object_response']
        return msgpack.unpackb(gio['object'], use_list=False, raw=False)


class IssObjectMetadataCommand(Command):
    def __init__(self, context_id: str, object_id: str):
        super().__init__(context_id)
        self.object_id = object_id

    def name(self) -> str:
        return 'iss_object_metadata'

    def create_content(self) -> Content:
        return \
            {
                'iss_object_metadata': {
                    'context_id': self.context_id,
                    'object_id': self.object_id
                }
            }

    def decode_content(self, content: Content) -> Object:
        giom = content['iss_object_metadata_response']
        return giom['metadata']


class PostIssObjectCommand(Command):
    def __init__(self, context_id: str, schema: str, obj: Object):
        super().__init__(context_id)
        self.schema = schema
        self.obj = obj

    def name(self) -> str:
        return 'post_iss_object'

    def create_content(self) -> Content:
        return \
            {
                'post_iss_object': {
                    'context_id': self.context_id,
                    'schema': self.schema,
                    'encoding': 'msgpack',
                    'object': msgpack.packb(self.obj, use_bin_type=True)
                }
            }

    def decode_content(self, content: Content) -> Object:
        pio = content['post_iss_object_response']
        return pio['object_id']


class PostCalculationCommand(Command):
    def __init__(self, context_id: str, calculation: Object):
        super().__init__(context_id)
        self.calculation = calculation

    def name(self) -> str:
        return 'post_calculation'

    def create_content(self) -> Content:
        return \
            {
                'post_calculation': {
                    'context_id': self.context_id,
                    'calculation': self.calculation
                }
            }

    def decode_content(self, content: Content) -> Object:
        pc = content['post_calculation_response']
        return pc['calculation_id']


class PerformLocalCalcCommand(Command):
    def __init__(self, context_id: str, calculation: Object):
        super().__init__(context_id)
        self.calculation = calculation

    def name(self) -> str:
        return 'perform_local_calc'

    def create_content(self) -> Content:
        return \
            {
                'perform_local_calc': {
                    'context_id': self.context_id,
                    'calculation': self.calculation
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['local_calc_result']


class IssDiffCommand(Command):
    def __init__(self, context_id: str, objA_id: str, objB_id: str):
        super().__init__(context_id)
        self.objA_id = objA_id
        self.objB_id = objB_id

    def name(self) -> str:
        return 'iss_diff'

    def create_content(self) -> Content:
        return \
            {
                'iss_diff': {
                    'id_a': self.objA_id,
                    'context_a': self.context_id,
                    'id_b': self.objB_id,
                    'context_b': self.context_id
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['iss_diff_response']


class CalculationDiffCommand(Command):
    def __init__(self, context_id: str, calcA_id: str, calcB_id: str):
        super().__init__(context_id)
        self.calcA_id = calcA_id
        self.calcB_id = calcB_id

    def name(self) -> str:
        return 'calculation_diff'

    def create_content(self) -> Content:
        return \
            {
                'calculation_diff': {
                    'id_a': self.calcA_id,
                    'context_a': self.context_id,
                    'id_b': self.calcB_id,
                    'context_b': self.context_id
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['calculation_diff_response']


class CalculationRequestCommand(Command):
    def __init__(self, context_id: str, calc_id: str):
        super().__init__(context_id)
        self.calc_id = calc_id

    def name(self) -> str:
        return 'calculation_request'

    def create_content(self) -> Content:
        return \
            {
                'calculation_request': {
                    'context_id': self.context_id,
                    'calculation_id': self.calc_id
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['calculation_request_response']


class IntrospectionStatusCommand(Command):
    def __init__(self, context_id: str, include_finished: bool):
        super().__init__(context_id)
        self.include_finished = include_finished

    def name(self) -> str:
        return 'introspection_status_query'

    def create_content(self) -> Content:
        return \
            {
                'introspection_status_query': {
                    'include_finished': self.include_finished
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['introspection_status_response']


class IntrospectionSetEnabledCommand(Command):
    def __init__(self, context_id: str, enabled: bool):
        super().__init__(context_id)
        self.enabled = enabled

    def name(self) -> str:
        return 'introspection_control set enabled'

    def create_content(self) -> Content:
        return \
            {
                'introspection_control': {
                    'enabled': self.enabled
                }
            }

    def decode_content(self, _content: Content) -> None:
        return None


class IntrospectionClearAdminCommand(Command):
    def name(self) -> str:
        return 'introspection_control clear admin'

    def create_content(self) -> Content:
        return \
            {
                'introspection_control': {
                    'clear_admin': None
                }
            }

    def decode_content(self, _content: Content) -> None:
        return None


class QueryRequestsMetaInfoCommand(Command):
    def name(self) -> str:
        return 'query requests meta info'

    def create_content(self) -> Content:
        return \
            {
                'requests_meta_info_query': {
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['requests_meta_info_response']


class ResolveRequestCommand(Command):
    def __init__(self, context_id: str, result_type: str, req_data: Object):
        super().__init__(context_id)
        self.result_type = result_type
        self.json_text = json.dumps(req_data)

    def name(self) -> str:
        return 'resolve request'

    def create_content(self) -> Content:
        return \
            {
                'resolve_request': {
                    'result_type': self.result_type,
                    'json_text': self.json_text
                }
            }

    def decode_content(self, content: Content) -> Object:
        return content['resolve_request_response']
