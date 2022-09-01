""" Generates configuration files for Home Assistant based on passed device name """
import sys  # needed to use arguments

NAME = sys.argv[1]
ID = NAME.lower()
ID = ID.replace(' ', '_')

print('NAME: ' + NAME)
print('ID: ' + ID)

genFilePrefix = 'th_'

templateFolder = 'templates/'
autoFileTemplate = templateFolder + 'thermostat_automation_template.yaml'
historyStatsTemplate = templateFolder + 'thermostat_history_stats_template.yaml'

outfolder = 'out/'
autoFile = outfolder + genFilePrefix + 'automation_' + ID + '.yaml'
histFile = outfolder + genFilePrefix + 'sensor_history_stats_' + ID + '.yaml'

listOfTemplates = [autoFileTemplate, historyStatsTemplate]
listOfNewFiles = [autoFile, histFile]
writeFile = False

# generate new files
for templateFile in listOfTemplates:
    idx = listOfTemplates.index(templateFile)
    with open(listOfNewFiles[idx], 'w+', encoding="utf-8") as output_file, open(templateFile, 'r', encoding="utf-8") as input_file:
        data = input_file.read()
        data = data.replace('{{ID}}', ID)
        data = data.replace('{{NAME}}', NAME)
        output_file.write(data)
        input_file.close()
        output_file.close()
