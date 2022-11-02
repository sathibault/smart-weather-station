import math
import numpy as np

def normalize(x, norm):
    return 2*(x-norm['base'])/norm['spread'] - 1

def unnormalize(y, norm):
    return norm['spread']*(y+1)/2 + norm['base']

def act(x):
    #return (math.exp(2*x)-1) / (math.exp(2*x)+1)
    return 2.0/(1.0+math.exp(-2.0*x)) - 1.0

actv = np.vectorize(act)

def sigmoid(x):
    return 1.0/(1.0+math.exp(-x))

sigmoidv = np.vectorize(sigmoid)

class BRNN:
    def __init__(self, features, theta, x_norm, y_norm, binary=False):
        self.features = features
        self.theta = theta
        self.x_norm = x_norm
        self.y_norm = y_norm
        self.binary = binary

        self.W1 = theta[:,2:].T
        self.bias1 = theta[:,1].T
        self.W2 = theta[:,0].T

        self.norm = lambda x: normalize(x, self.x_norm)
        self.unnorm = lambda y: unnormalize(y, self.y_norm)

    def predict(self, X):
        Xn = np.apply_along_axis(self.norm, 1, X)
        o1 = np.matmul(Xn, self.W1) + self.bias1
        a1 = actv(o1)
        o2 = np.matmul(a1, self.W2)
        y = self.unnorm(o2)
        return sigmoidv(y) if self.binary else y
