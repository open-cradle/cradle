"""Interface to CRADLE."""

import sys
from typing import Optional

import cradle.command as cmd
import cradle.connection as conn


class Session:
    """Session with the CRADLE server."""
    def __init__(self):
        self.impl = conn.Connection()
        self.context_id = self.impl.context_id
        self._git_version = None
        self.registration()

    @property
    def api_url(self) -> str:
        return self.impl.api_url

    @property
    def git_version(self) -> str:
        if self._git_version is None:
            self._git_version = self.query_requests_meta_info()['git_version']
        return self._git_version

    def registration(self) -> None:
        command = cmd.RegistrationCommand(self.context_id,
                                          self.impl.api_url, self.impl.api_token)
        self.impl.perform_command(command)

    def get_iss_object(self, object_id: str, ignore_upgrades: bool = False) -> cmd.Object:
        """Retrieve an object from the ISS."""
        print('FETCHING ' + object_id, file=sys.stderr)
        command = cmd.IssObjectCommand(self.context_id, object_id, ignore_upgrades)
        return self.impl.perform_command(command)

    def get_iss_object_metadata(self, object_id: str) -> cmd.Object:
        """Retrieve an ISS object's metadata."""
        print('FETCHING metadata for ' + object_id, file=sys.stderr)
        command = cmd.IssObjectMetadataCommand(self.context_id, object_id)
        return self.impl.perform_command(command)

    def post_iss_object(self, schema: str, obj: cmd.Object) -> str:
        """Post an ISS object and return its ID."""
        print('POSTING ISS OBJECT', file=sys.stderr)
        command = cmd.PostIssObjectCommand(self.context_id, schema, obj)
        return self.impl.perform_command(command)

    def post_calculation(self, calculation: cmd.Object) -> str:
        """Post a calculation and return its ID."""
        print('POSTING CALCULATION', file=sys.stderr)
        command = cmd.PostCalculationCommand(self.context_id, calculation)
        return self.impl.perform_command(command)

    def perform_local_calc(self, calculation: cmd.Object, num_requests: int = 1) -> cmd.Object:
        """Perform a local calculation and return its result."""
        print('PERFORMING LOCAL CALC', file=sys.stderr)
        command = cmd.PerformLocalCalcCommand(self.context_id, calculation)
        response = None
        for _ in range(num_requests):
            self.impl.send_request(command)
        for _ in range(num_requests):
            response = self.impl.receive_response(command)
        return response

    def start_local_calc(self, calculation: cmd.Object) -> cmd.Command:
        """Start a local calculation."""
        print('START LOCAL CALC', file=sys.stderr)
        command = cmd.PerformLocalCalcCommand(self.context_id, calculation)
        self.impl.send_request(command)
        return command

    def iss_diff(self, objA_id: str, objB_id: str) -> cmd.Object:
        """
        Compute the diff between two objects (in the same realm).
        """
        print('ISS DIFF', file=sys.stderr)
        command = cmd.IssDiffCommand(self.context_id, objA_id, objB_id)
        return self.impl.perform_command(command)

    def calculation_diff(self, calcA_id: str, calcB_id: str) -> cmd.Object:
        """
        Compute the diff between two calculations (in the same realm).
        """
        print('CALCULATION DIFF', file=sys.stderr)
        command = cmd.CalculationDiffCommand(self.context_id, calcA_id, calcB_id)
        return self.impl.perform_command(command)

    def calculation_request(self, calc_id: str) -> cmd.Object:
        """Retrieve the calculation descriptor for a calculation id."""
        print('CALCULATION REQUEST', file=sys.stderr)
        command = cmd.CalculationRequestCommand(self.context_id, calc_id)
        return self.impl.perform_command(command)

    def start_calculation_request(self, calc_id: str) -> cmd.Command:
        """Retrieve the calculation descriptor for a calculation id."""
        print('START CALCULATION REQUEST', file=sys.stderr)
        command = cmd.CalculationRequestCommand(self.context_id, calc_id)
        self.impl.send_request(command)
        return command

    def query_introspection_status(self, include_finished: bool) -> cmd.Object:
        print('INTROSPECTION STATUS', file=sys.stderr)
        command = cmd.IntrospectionStatusCommand(self.context_id, include_finished)
        return self.impl.perform_command(command)

    def introspection_set_enabled(self, enabled: bool) -> None:
        print(f'INTROSPECTION SET ENABLED {enabled}', file=sys.stderr)
        command = cmd.IntrospectionSetEnabledCommand(self.context_id, enabled)
        self.impl.perform_command(command)

    def introspection_clear_admin(self) -> None:
        print('INTROSPECTION CONTROL CLEAR', file=sys.stderr)
        command = cmd.IntrospectionClearAdminCommand(self.context_id)
        self.impl.perform_command(command)

    def receive_response(self, command) -> Optional[cmd.Object]:
        """Receive a response for a started request (whatever its type)."""
        print('RECEIVE RESPONSE', file=sys.stderr)
        return self.impl.receive_response(command)

    def query_requests_meta_info(self) -> cmd.Object:
        '''Retrieve info on the requests subsystem'''
        print('QUERY REQUESTS META INFO', file=sys.stderr)
        command = cmd.QueryRequestsMetaInfoCommand(self.context_id)
        return self.impl.perform_command(command)

    def resolve_request(self, req_data: cmd.Object, remote: bool) -> cmd.Object:
        print(f'RESOLVE REQUEST {remote=}', file=sys.stderr)
        command = cmd.ResolveRequestCommand(self.context_id, req_data, remote)
        return self.impl.perform_command(command)
