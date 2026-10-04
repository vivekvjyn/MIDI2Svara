import torch


class Block(torch.nn.Module):
    def __init__(self, channels, kernel):
        super().__init__()
        self.conv_block1 = torch.nn.Sequential(
            torch.nn.Conv1d(channels, channels, kernel_size=kernel,
                            stride=1, padding="same"),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )
        self.conv_block2 = torch.nn.Sequential(
            torch.nn.Conv1d(channels, channels, kernel_size=3,
                            stride=1, padding="same"),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )

    def forward(self, identity):
        x = self.conv_block1(identity)
        x = self.conv_block2(x)
        return x + identity


class RagaNet(torch.nn.Module):
    def __init__(self, params):
        super().__init__()
        channels = params["channels"]
        self.conv_first = torch.nn.Sequential(
            torch.nn.Conv1d(params["input"], channels, kernel_size=80,
                            stride=params["stride"]),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )
        self.res_blocks = torch.nn.ModuleList(
            [Block(channels, 3) for _ in range(params["blocks"])])
        self.fc1 = torch.nn.Linear(channels, params["classes"])
        self.max_pool_every = params["poolEvery"]

    def forward(self, x):
        x = self.conv_first(x)
        for index, block in enumerate(self.res_blocks):
            x = block(x)
            if index % self.max_pool_every == 0:
                x = torch.nn.functional.max_pool1d(x, 2)
        x = torch.nn.functional.avg_pool1d(x, x.shape[-1])
        x = x.permute(0, 2, 1)
        x = self.fc1(x)
        return torch.nn.functional.log_softmax(x, dim=-1)
