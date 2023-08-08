# custom filtering extensions

'''
These methods are applied by the following procedure:
    define the method with an argument for the incoming data
    add any required code to manipulate the data
    return the resultant data
    attach the new method
'''

# for custom processing before standard processing
#def custom_pre_process(data):
#    your code here...
#    return(line)
#self.custom_pre_process = custom_pre_process


# for custom parsing before standard parsing
#def custom_pre_parse(data):
#    your code here...
#    return(data)
#self.custom_pre_parse = custom_pre_parse


# for custom parsing after standard parsing
#def custom_post_parse(data):
#    your code here...
#    return(data)
#self.custom_post_parse = custom_post_parse


# override the original parse_code procedure
#def parse_code(data):
#    your code here...
#    return(data)
#self.parse_code = parse_code