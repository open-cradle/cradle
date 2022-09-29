def test_req_composite(session):
    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'polymorphic_name': 'rq_retrieve_immutable_object_func+gba882d4-dirty',
                  'ptr_wrapper': {'data': {'args': {'tuple_element0': 'https://mgh.thinknode.io/api/v1.0',
                                                    'tuple_element1': '123',
                                                    'tuple_element2': {'impl': {'polymorphic_id': 2147483650,
                                                                                'polymorphic_name': 'rq_resolve_iss_object_to_immutable_func+gba882d4-dirty',
                                                                                'ptr_wrapper': {'data': {'args': {'tuple_element0': 'https://mgh.thinknode.io/api/v1.0',
                                                                                                                  'tuple_element1': '123',
                                                                                                                  'tuple_element2': {'impl': {'polymorphic_id': 2147483651,
                                                                                                                                              'polymorphic_name': 'rq_post_iss_object_func+gba882d4-dirty',
                                                                                                                                              'ptr_wrapper': {'data': {'args': {'tuple_element0': 'https://mgh.thinknode.io/api/v1.0',
                                                                                                                                                                                'tuple_element1': '123',
                                                                                                                                                                                'tuple_element2': 'string',
                                                                                                                                                                                'tuple_element3': {'blob': 'cGF5bG9hZA==',
                                                                                                                                                                                                   'size': 7}},
                                                                                                                                                                       'uuid': {'value0': 'rq_post_iss_object_func+gba882d4-dirty'}},
                                                                                                                                                              'id': 2147483651}},
                                                                                                                                     'title': 'post_iss_object'},
                                                                                                                  'tuple_element3': True},
                                                                                                         'uuid': {'value0': 'rq_resolve_iss_object_to_immutable_func+gba882d4-dirty'}},
                                                                                                'id': 2147483650}},
                                                                       'title': 'resolve_iss_object_to_immutable'}},
                                           'uuid': {'value0': 'rq_retrieve_immutable_object_func+gba882d4-dirty'}},
                                  'id': 2147483649}},
         'title': 'retrieve_immutable_object'}

    # TODO solve rapidjson assert
    # result = session.resolve_request(req_data)
    # assert result == '61e8256000c030b6c41dffee788df15f'
