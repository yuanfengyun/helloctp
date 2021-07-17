import PySimpleGUI as sg
import sys

sg.theme('BluePurple')
sg.ChangeLookAndFeel('Dark')

# 创建一个参数输入对话框
layout = [[sg.Text('期货公司'), sg.Input("H宏源期货", key="broker_id")],
          [sg.Text('账       号'), sg.Input("111", key="user_id")],
          [sg.Text('密       码'), sg.Input("111", password_char="*",key="password")],
          [sg.OK(), sg.Cancel()]]
window = sg.Window('登陆', layout)

# 读取用户输入值
event, values = window.Read()
window.close()

if event != "OK":
    sys.exit()

platform = values["broker_id"]
account = values["user_id"]
password = values["password"]