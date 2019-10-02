import subprocess
import pytest
import msi_values

MSIx64_PATH = 'virtio-win-gtx64.msi'
MSIx86_PATH = 'virtio-win-gtx86.msi'


def run_command(command):
    try:
        return subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=True
        ).stdout.decode()
    except subprocess.CalledProcessError as e:
        raise Exception(
            "command: '{}' failed with '{}'".format(
                ' '.join(e.cmd),
                e.stdout)
        )


class Table():

    def __init__(self, name, table_raw_content):
        self.name = name
        self.colum_names_to_index = self._map_colum_names_to_index(
            table_raw_content
        )
        self._parse_raw_content(table_raw_content[1:])

    def _map_colum_names_to_index(self, content):
        return dict(
            zip(
                range(len(content[0])),
                content[0]
            )
        )

    def _parse_raw_content(self, content):
        self.rows = {}
        for row in content:
            self.rows[row[0]] = row[1:]

    def get_row(self, row_pk):
        return self.rows[row_pk]


class MSI(object):
    def __init__(self, msi_file, arch, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.msi_file = msi_file
        self.init_tables()
        self.arch = arch

    def init_tables(self):
        def _get_table_list():
            return filter(
                lambda table: not table.startswith("_"),
                run_command(
                    ["msiinfo",  "tables", self.msi_file]
                ).splitlines()
            )

        def _get_table_content(table_name):
            table_content = run_command(
                ["msiinfo", "export", self.msi_file, table_name]
            ).splitlines()
            return [
                row.split('\t')
                for row in
                [table_content[0]] + table_content[3:]
            ]

        self.tables = {}
        msi_table_list = _get_table_list()
        for table_name in msi_table_list:
            table_content = _get_table_content(table_name)
            self.tables[table_name] = Table(
                table_name,
                table_content
            )

    def get_table(self, table_name):
        return self.tables.get(table_name)

    def get_arch(self):
        return self.arch



@pytest.fixture(scope="session")
def get_msis():
    return [MSI(MSIx64_PATH, 'x64'), MSI(MSIx86_PATH, 'x86')]


def _generic_msi_value_test(msis, table, expected):
    for msi in msis:
        msi_table = msi.get_table(table)
        expected_values = expected[msi.get_arch()]
        for comp_id in expected_values.keys():
            assert all(
                [
                    val in msi_table.get_row(comp_id) 
                    for val in expected_values[comp_id]
                ])


def test_component_ids(get_msis):
    """
    To follow windows component rules we must make sure that the
    Component ID hasn't changed, if we do intend to change it,
    we must make sure to scheduale the majorupgrade early in the
    upgrade process and update the expected value
    """
    _ , expected_values = msi_values.get_expected_componentids()
    _generic_msi_value_test(
        get_msis,
        'Component',
        expected_values
    )


def test_upgrade_code(get_msis):
    """
    Upgradecode is used by msi to detect previus product versions
    in general it shouldn't change, if it does change please make
    sure that the previus product get uninstalled first to prevent
    problems and update the expected value
    """
    _ , expected_values = msi_values.get_expected_upgrade_code()
    _generic_msi_value_test(
        get_msis,
        'Property',
        expected_values
    )


def test_allusers(get_msis):
    _ , expected_values = msi_values.get_expected_allusers_property()
    _generic_msi_value_test(
        get_msis,
        'Property',
        expected_values
    )


def test_old_wgt_uninstall_path(get_msis):
    _ , expected_values = msi_values.get_expected_RegLocator()
    _generic_msi_value_test(
        get_msis,
        'RegLocator',
        expected_values
    )


def test_Registry(get_msis):
    _ , expected_values = msi_values.get_expected_registry()
    _generic_msi_value_test(
        get_msis,
        'Registry',
        expected_values
    )

if __name__ == "__main__":
    test_component_ids(get_msis())
